| `THROUGHPUT_LIMIT` | `DeviceLinkThroughputLimit` | `U32` | bytes/s | 500000000 | `DeviceLinkThroughputLimit` | register table |
| `BANDWIDTH_RESERVE` | `DeviceLinkBandwidthReserve` | `F64` | percent | — | `DeviceLinkBandwidthReserve` | register table |
| `INDICATOR_MODE` | `DeviceIndicatorMode` | `IndicatorMode` | — | — | `DeviceIndicatorMode` | register table |
| `PIXEL_FORMAT` | `PixelFormat` | `PixelFormat` | — | BayerRG8 | `PixelFormat` | register table |
| `ADC_BIT_DEPTH` | `AdcBitDepth` | `AdcBitDepth` | — | — | `AdcBitDepth` | register table |
| `ISP_ENABLE` | `IspEnable` | `bool` | — | — | `IspEnable` | register table |
| `BINNING_HORIZONTAL_MODE` | `BinningHorizontalMode` | `BinningMode` | — | — | `BinningHorizontalMode` | register table |
| `BINNING_VERTICAL_MODE` | `BinningVerticalMode` | `BinningMode` | — | — | `BinningVerticalMode` | register table |
| `BINNING_HORIZONTAL` | `BinningHorizontal` | `U32` | — | — | `BinningHorizontal` | register table |
| `BINNING_VERTICAL` | `BinningVertical` | `U32` | — | — | `BinningVertical` | register table |
| `DECIMATION_HORIZONTAL` | `DecimationHorizontal` | `U32` | — | — | `DecimationHorizontal` | register table |
| `DECIMATION_VERTICAL` | `DecimationVertical` | `U32` | — | — | `DecimationVertical` | register table |
| `WIDTH` | `Width` | `U32` | pixels | — | `Width` | register table |
| `HEIGHT` | `Height` | `U32` | pixels | — | `Height` | register table |
| `OFFSET_X` | `OffsetX` | `U32` | pixels | — | `OffsetX` | register table |
| `OFFSET_Y` | `OffsetY` | `U32` | pixels | — | `OffsetY` | register table |
| `REVERSE_X` | `ReverseX` | `bool` | — | — | `ReverseX` | register table |
| `REVERSE_Y` | `ReverseY` | `bool` | — | — | `ReverseY` | register table |
| `TEST_PATTERN` | `TestPattern` | `TestPattern` | — | — | `TestPattern` | register table |
| `EXPOSURE_MODE` | `ExposureMode` | `ExposureMode` | — | — | `ExposureMode` | register table |
| `SHUTTER_MODE` | `SensorShutterMode` | `ShutterMode` | — | — | `SensorShutterMode` | register table |
| `EXPOSURE_AUTO` | `ExposureAuto` | `AutoMode` | — | Off | `ExposureAuto` | register table |
| `EXPOSURE_TIME` | `ExposureTime` | `F64` | us | 0 | `ExposureTime` | register table |
| `TRIGGER_MODE` | `TriggerMode` | `TriggerMode` | — | — | `TriggerMode` | register table |
| `TRIGGER_SOURCE` | `TriggerSource` | `TriggerSource` | — | — | `TriggerSource` | register table |
| `TRIGGER_ACTIVATION` | `TriggerActivation` | `TriggerActivation` | — | — | `TriggerActivation` | register table |
| `TRIGGER_OVERLAP` | `TriggerOverlap` | `TriggerOverlap` | — | — | `TriggerOverlap` | register table |
| `TRIGGER_DELAY` | `TriggerDelay` | `F64` | us | — | `TriggerDelay` | register table |
| `GAIN_AUTO` | `GainAuto` | `AutoMode` | — | Off | `GainAuto` | register table |
| `GAIN` | `Gain` | `F64` | dB | 0 | `Gain` | register table |
| `BLACK_LEVEL` | `BlackLevel` | `F64` | percent | — | `BlackLevel` | register table |
| `BLACK_LEVEL_CLAMPING_ENABLE` | `BlackLevelClampingEnable` | `bool` | — | — | `BlackLevelClampingEnable` | register table |
| `GAMMA_ENABLE` | `GammaEnable` | `bool` | — | — | `GammaEnable` | register table |
| `GAMMA` | `Gamma` | `F64` | — | — | `Gamma` | register table |
| `BALANCE_WHITE_AUTO` | `BalanceWhiteAuto` | `AutoMode` | — | — | `BalanceWhiteAuto` | register table |
| `BALANCE_RATIO_RED` | `BalanceRatio[Red]` | `F64` | — | — | `BalanceRatioRed` | register table |
| `BALANCE_RATIO_BLUE` | `BalanceRatio[Blue]` | `F64` | — | — | `BalanceRatioBlue` | register table |
| `AUTO_EXPOSURE_TARGET_GREY_VALUE_AUTO` | `AutoExposureTargetGreyValueAuto` | `TargetGreyValueAuto` | — | — | `AutoExposureTargetGreyValueAuto` | Table 1 name |
| `AUTO_EXPOSURE_TARGET_GREY_VALUE` | `AutoExposureTargetGreyValue` | `F64` | percent | — | `AutoExposureTargetGreyValue` | Table 1 name |
| `AUTO_EXPOSURE_LIGHTING_MODE` | `AutoExposureLightingMode` | `LightingMode` | — | — | `AutoExposureLightingMode` | Table 1 name |
| `AUTO_EXPOSURE_METERING_MODE` | `AutoExposureMeteringMode` | `MeteringMode` | — | — | `AutoExposureMeteringMode` | Table 1 name |
| `AUTO_EXPOSURE_EXPOSURE_TIME_LOWER_LIMIT` | `AutoExposureExposureTimeLowerLimit` | `F64` | us | — | `AutoExposureExposureTimeLowerLimit` | Table 1 name |
| `AUTO_EXPOSURE_EXPOSURE_TIME_UPPER_LIMIT` | `AutoExposureExposureTimeUpperLimit` | `F64` | us | — | `AutoExposureExposureTimeUpperLimit` | Table 1 name |
| `AUTO_EXPOSURE_GAIN_LOWER_LIMIT` | `AutoExposureGainLowerLimit` | `F64` | dB | — | `AutoExposureGainLowerLimit` | Table 1 name |
| `AUTO_EXPOSURE_GAIN_UPPER_LIMIT` | `AutoExposureGainUpperLimit` | `F64` | dB | — | `AutoExposureGainUpperLimit` | Table 1 name |
| `AUTO_EXPOSURE_GREY_VALUE_LOWER_LIMIT` | `AutoExposureGreyValueLowerLimit` | `F64` | percent | — | `AutoExposureGreyValueLowerLimit` | Table 1 name |
| `AUTO_EXPOSURE_GREY_VALUE_UPPER_LIMIT` | `AutoExposureGreyValueUpperLimit` | `F64` | percent | — | `AutoExposureGreyValueUpperLimit` | Table 1 name |
| `AUTO_EXPOSURE_EV_COMPENSATION` | `AutoExposureEVCompensation` | `F64` | — | — | `AutoExposureEVCompensation` | Table 1 name |
| `AUTO_EXPOSURE_CONTROL_LOOP_DAMPING` | `AutoExposureControlLoopDamping` | `F64` | — | — | `AutoExposureControlLoopDamping` | Table 1 name |
| `AUTO_EXPOSURE_CONTROL_PRIORITY` | `AutoExposureControlPriority` | `ControlPriority` | — | — | `AutoExposureControlPriority` | Table 1 name |
| `AAS_ROI_ENABLE` | `AasRoiEnable[Ae]` | `bool` | — | — | `AasRoiEnable` | Table 1 name |
| `AAS_ROI_OFFSET_X` | `AasRoiOffsetX[Ae]` | `U32` | pixels | — | `AasRoiOffsetX` | Table 1 name |
| `AAS_ROI_OFFSET_Y` | `AasRoiOffsetY[Ae]` | `U32` | pixels | — | `AasRoiOffsetY` | Table 1 name |
| `AAS_ROI_WIDTH` | `AasRoiWidth[Ae]` | `U32` | pixels | — | `AasRoiWidth` | Table 1 name |
| `AAS_ROI_HEIGHT` | `AasRoiHeight[Ae]` | `U32` | pixels | — | `AasRoiHeight` | Table 1 name |
| `BALANCE_WHITE_AUTO_PROFILE` | `BalanceWhiteAutoProfile` | `WhiteBalanceProfile` | — | — | `BalanceWhiteAutoProfile` | Table 1 name |
| `BALANCE_WHITE_AUTO_DAMPING` | `BalanceWhiteAutoDamping` | `F64` | — | — | `BalanceWhiteAutoDamping` | Table 1 name |
| `BALANCE_WHITE_AUTO_LOWER_LIMIT` | `BalanceWhiteAutoLowerLimit` | `F64` | — | — | `BalanceWhiteAutoLowerLimit` | Table 1 name |
| `BALANCE_WHITE_AUTO_UPPER_LIMIT` | `BalanceWhiteAutoUpperLimit` | `F64` | — | — | `BalanceWhiteAutoUpperLimit` | Table 1 name |
| `EXPOSURE_ACTIVE_MODE` | `ExposureActiveMode` | `ExposureActiveMode` | — | — | `ExposureActiveMode` | Table 1 name |
| `COLOR_TRANSFORMATION_ENABLE` | `ColorTransformationEnable` | `bool` | — | — | `ColorTransformationEnable` | Table 1 name |
| `RGB_TRANSFORM_LIGHT_SOURCE` | `RgbTransformLightSource` | `LightSource` | — | — | `RgbTransformLightSource` | Table 1 name |
| `SATURATION_ENABLE` | `SaturationEnable` | `bool` | — | — | `SaturationEnable` | Table 1 name |
| `SATURATION` | `Saturation` | `F64` | — | — | `Saturation` | Table 1 name |
| `SHARPENING_ENABLE` | `SharpeningEnable` | `bool` | — | — | `SharpeningEnable` | Table 1 name |
| `SHARPENING_AUTO` | `SharpeningAuto` | `bool` | — | — | `SharpeningAuto` | Table 1 name |
| `SHARPENING` | `Sharpening` | `F64` | — | — | `Sharpening` | Table 1 name |
| `SHARPENING_THRESHOLD` | `SharpeningThreshold` | `F64` | — | — | `SharpeningThreshold` | Table 1 name |
| `DEFECT_CORRECT_STATIC_ENABLE` | `DefectCorrectStaticEnable` | `bool` | — | — | `DefectCorrectStaticEnable` | Table 1 name |
| `DEFECT_CORRECTION_MODE` | `DefectCorrectionMode` | `DefectCorrectionMode` | — | — | `DefectCorrectionMode` | Table 1 name |
| `FRAME_RATE_ENABLE` | `AcquisitionFrameRateEnable` | `bool` | — | true | `AcquisitionFrameRateEnable` | register table |
| `FRAME_RATE` | `AcquisitionFrameRate` | `F64` | Hz | 0 | `AcquisitionFrameRate` | register table |
