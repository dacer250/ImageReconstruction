/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#if defined(_MSC_VER)
#pragma warning ( disable : 4786 )
#endif

#include "itkImageRegistrationMethod.h"
#include "itkMeanSquaresImageToImageMetric.h"
#include "itkRegularStepGradientDescentOptimizer.h"
#include "itkCenteredTransformInitializer.h"
#include "itkAffineTransform.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkResampleImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkSubtractImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"


//
//  The following piece of code implements an observer
//  that will monitor the evolution of the registration process.
//
#include "itkCommand.h"
class CommandIterationUpdate : public itk::Command
{
public:
  typedef  CommandIterationUpdate   Self;
  typedef  itk::Command             Superclass;
  typedef itk::SmartPointer<Self>   Pointer;
  itkNewMacro( Self );
protected:
  CommandIterationUpdate() {};
public:
  typedef itk::RegularStepGradientDescentOptimizer OptimizerType;
  typedef   const OptimizerType *                  OptimizerPointer;

  void Execute(itk::Object *caller, const itk::EventObject & event)
    {
    Execute( (const itk::Object *)caller, event);
    }

  void Execute(const itk::Object * object, const itk::EventObject & event)
    {
    OptimizerPointer optimizer =
                      dynamic_cast< OptimizerPointer >( object );
    if( ! itk::IterationEvent().CheckEvent( &event ) )
      {
      return;
      }
      std::cout << optimizer->GetCurrentIteration() << "   ";
      std::cout << optimizer->GetValue() << "   ";
      std::cout << optimizer->GetCurrentPosition();

      // Print the angle for the trace plot
      vnl_matrix<double> p(2, 2);
      p[0][0] = (double) optimizer->GetCurrentPosition()[0];
      p[0][1] = (double) optimizer->GetCurrentPosition()[1];
      p[1][0] = (double) optimizer->GetCurrentPosition()[2];
      p[1][1] = (double) optimizer->GetCurrentPosition()[3];
      vnl_svd<double> svd(p);
      vnl_matrix<double> r(2, 2);
      r = svd.U() * vnl_transpose(svd.V());
      double angle = vcl_asin(r[1][0]);
      std::cout << " AffineAngle: " << angle * 180.0 / vnl_math::pi << std::endl;
    }
};


int main( int argc, char *argv[] )
{
  if( argc < 4 )
    {
    std::cerr << "Missing Parameters " << std::endl;
    std::cerr << "Usage: " << argv[0];
    std::cerr << "   fixedImageFile  movingImageFile " << std::endl;
    std::cerr << "   outputImagefile  [differenceBeforeRegistration] " << std::endl;
    std::cerr << "   [differenceAfterRegistration] " << std::endl;
    std::cerr << "   [stepLength] [maxNumberOfIterations] "<< std::endl;
    return EXIT_FAILURE;
    }

  const    unsigned int    Dimension = 2;
  typedef  float           PixelType;

  typedef itk::Image< PixelType, Dimension >  FixedImageType;
  typedef itk::Image< PixelType, Dimension >  MovingImageType;

  typedef itk::AffineTransform<
                                  double,
                                  Dimension  >     TransformType;

  typedef itk::RegularStepGradientDescentOptimizer       OptimizerType;
  typedef itk::MeanSquaresImageToImageMetric<
                                    FixedImageType,
                                    MovingImageType >    MetricType;
  typedef itk:: LinearInterpolateImageFunction<
                                    MovingImageType,
                                    double          >    InterpolatorType;
  typedef itk::ImageRegistrationMethod<
                                    FixedImageType,
                                    MovingImageType >    RegistrationType;

  MetricType::Pointer         metric        = MetricType::New();
  OptimizerType::Pointer      optimizer     = OptimizerType::New();
  InterpolatorType::Pointer   interpolator  = InterpolatorType::New();
  RegistrationType::Pointer   registration  = RegistrationType::New();

  registration->SetMetric(        metric        );
  registration->SetOptimizer(     optimizer     );
  registration->SetInterpolator(  interpolator  );

  TransformType::Pointer  transform = TransformType::New();
  registration->SetTransform( transform );

  typedef itk::ImageFileReader< FixedImageType  > FixedImageReaderType;
  typedef itk::ImageFileReader< MovingImageType > MovingImageReaderType;

  FixedImageReaderType::Pointer  fixedImageReader  = FixedImageReaderType::New();
  MovingImageReaderType::Pointer movingImageReader = MovingImageReaderType::New();

  fixedImageReader->SetFileName(  argv[1] );
  movingImageReader->SetFileName( argv[2] );

  registration->SetFixedImage(    fixedImageReader->GetOutput()    );
  registration->SetMovingImage(   movingImageReader->GetOutput()   );

  fixedImageReader->Update();

  registration->SetFixedImageRegion(
     fixedImageReader->GetOutput()->GetBufferedRegion() );

  typedef itk::CenteredTransformInitializer<
                                    TransformType,
                                    FixedImageType,
                                    MovingImageType >  TransformInitializerType;

  TransformInitializerType::Pointer initializer = TransformInitializerType::New();

  initializer->SetTransform(   transform );
  initializer->SetFixedImage(  fixedImageReader->GetOutput() );
  initializer->SetMovingImage( movingImageReader->GetOutput() );
  initializer->MomentsOn();
  initializer->InitializeTransform();


  registration->SetInitialTransformParameters(
                                 transform->GetParameters() );


  double translationScale = 1.0 / 1000.0;
  if( argc > 8 )
    {
    translationScale = atof( argv[8] );
    }

  typedef OptimizerType::ScalesType       OptimizerScalesType;
  OptimizerScalesType optimizerScales( transform->GetNumberOfParameters() );

  optimizerScales[0] =  1.0;
  optimizerScales[1] =  1.0;
  optimizerScales[2] =  1.0;
  optimizerScales[3] =  1.0;
  optimizerScales[4] =  translationScale;
  optimizerScales[5] =  translationScale;

  optimizer->SetScales( optimizerScales );

  double steplength = 0.1;

  if( argc > 6 )
    {
    steplength = atof( argv[6] );
    }

  unsigned int maxNumberOfIterations = 300;

  if( argc > 7 )
    {
    maxNumberOfIterations = atoi( argv[7] );
    }


  optimizer->SetMaximumStepLength( steplength );
  optimizer->SetMinimumStepLength( 0.0001 );
  optimizer->SetNumberOfIterations( maxNumberOfIterations );

  optimizer->MinimizeOn();

  CommandIterationUpdate::Pointer observer = CommandIterationUpdate::New();
  optimizer->AddObserver( itk::IterationEvent(), observer );

  try
    {
    registration->StartRegistration();
    std::cout << "Optimizer stop condition: "
              << registration->GetOptimizer()->GetStopConditionDescription()
              << std::endl;
    }
  catch( itk::ExceptionObject & err )
    {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << err << std::endl;
    return EXIT_FAILURE;
    }



  OptimizerType::ParametersType finalParameters =
                    registration->GetLastTransformParameters();

  const double finalRotationCenterX = transform->GetCenter()[0];
  const double finalRotationCenterY = transform->GetCenter()[1];
  const double finalTranslationX    = finalParameters[4];
  const double finalTranslationY    = finalParameters[5];

  const unsigned int numberOfIterations = optimizer->GetCurrentIteration();
  const double bestValue = optimizer->GetValue();


  // Print out results
  //
  std::cout << "Result = " << std::endl;
  std::cout << " Center X      = " << finalRotationCenterX  << std::endl;
  std::cout << " Center Y      = " << finalRotationCenterY  << std::endl;
  std::cout << " Translation X = " << finalTranslationX  << std::endl;
  std::cout << " Translation Y = " << finalTranslationY  << std::endl;
  std::cout << " Iterations    = " << numberOfIterations << std::endl;
  std::cout << " Metric value  = " << bestValue          << std::endl;

  //Compute the rotation angle and scaling from SVD of the matrix
  // \todo Find a way to figure out if the scales are along X or along Y.
  // VNL returns the eigenvalues ordered from largest to smallest.

  vnl_matrix<double> p(2, 2);
  p[0][0] = (double) finalParameters[0];
  p[0][1] = (double) finalParameters[1];
  p[1][0] = (double) finalParameters[2];
  p[1][1] = (double) finalParameters[3];
  vnl_svd<double> svd(p);
  vnl_matrix<double> r(2, 2);
  r = svd.U() * vnl_transpose(svd.V());
  double angle = vcl_asin(r[1][0]);

  const double angleInDegrees = angle * 180.0 / vnl_math::pi;

  std::cout << " Scale 1         = " << svd.W(0)        << std::endl;
  std::cout << " Scale 2         = " << svd.W(1)        << std::endl;
  std::cout << " Angle (degrees) = " << angleInDegrees  << std::endl;

  typedef itk::ResampleImageFilter<
                            MovingImageType,
                            FixedImageType >    ResampleFilterType;

  TransformType::Pointer finalTransform = TransformType::New();

  finalTransform->SetParameters( finalParameters );
  finalTransform->SetFixedParameters( transform->GetFixedParameters() );

  ResampleFilterType::Pointer resampler = ResampleFilterType::New();

  resampler->SetTransform( finalTransform );
  resampler->SetInput( movingImageReader->GetOutput() );

  FixedImageType::Pointer fixedImage = fixedImageReader->GetOutput();

  resampler->SetSize(    fixedImage->GetLargestPossibleRegion().GetSize() );
  resampler->SetOutputOrigin(  fixedImage->GetOrigin() );
  resampler->SetOutputSpacing( fixedImage->GetSpacing() );
  resampler->SetOutputDirection( fixedImage->GetDirection() );
  resampler->SetDefaultPixelValue( 100 );

  typedef  unsigned char  OutputPixelType;

  typedef itk::Image< OutputPixelType, Dimension > OutputImageType;

  typedef itk::CastImageFilter<
                        FixedImageType,
                        OutputImageType > CastFilterType;

  typedef itk::ImageFileWriter< OutputImageType >  WriterType;


  WriterType::Pointer      writer =  WriterType::New();
  CastFilterType::Pointer  caster =  CastFilterType::New();


  writer->SetFileName( argv[3] );


  caster->SetInput( resampler->GetOutput() );
  writer->SetInput( caster->GetOutput()   );
  writer->Update();


  typedef itk::SubtractImageFilter<
                                  FixedImageType,
                                  FixedImageType,
                                  FixedImageType > DifferenceFilterType;

  DifferenceFilterType::Pointer difference = DifferenceFilterType::New();

  difference->SetInput1( fixedImageReader->GetOutput() );
  difference->SetInput2( resampler->GetOutput() );

  WriterType::Pointer writer2 = WriterType::New();

  typedef itk::RescaleIntensityImageFilter<
                                  FixedImageType,
                                  OutputImageType >   RescalerType;

  RescalerType::Pointer intensityRescaler = RescalerType::New();

  intensityRescaler->SetInput( difference->GetOutput() );
  intensityRescaler->SetOutputMinimum(   0 );
  intensityRescaler->SetOutputMaximum( 255 );

  writer2->SetInput( intensityRescaler->GetOutput() );
  resampler->SetDefaultPixelValue( 1 );

  // Compute the difference image between the
  // fixed and resampled moving image.
  if( argc > 5 )
    {
    writer2->SetFileName( argv[5] );
    writer2->Update();
    }


  typedef itk::IdentityTransform< double, Dimension > IdentityTransformType;
  IdentityTransformType::Pointer identity = IdentityTransformType::New();

  // Compute the difference image between the
  // fixed and moving image before registration.
  if( argc > 4 )
    {
    resampler->SetTransform( identity );
    writer2->SetFileName( argv[4] );
    writer2->Update();
    }

  return EXIT_SUCCESS;
}