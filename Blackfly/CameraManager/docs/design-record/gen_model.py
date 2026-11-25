#!/usr/bin/env python3
"""Design-time generator for the camera feature part of the CameraManager model.

Reads the pathfinder's generated register table (JSON, never the vendor XML) and the
feature list below (SSDD Table 1), and writes:
  CameraFeatureTypes.fpp   enumerations of the camera features
  CameraParams.fppi        one parameter per Table 1 feature
  CameraReadBack.fppi      one read-back telemetry channel per Table 1 feature
  docs/design-record/parameter-table.md   the SDD parameter table rows
Usage: gen_model.py <blackfly_regtable.json> <CameraManager directory>
Source marks: T = feature is in the committed register table; R = named in SSDD Table 1 and
typed from the vendor's public model reference, to be confirmed when the table is regenerated.
"""
import json, re, sys

# (feature, parameter, type, unit, default, source); type "enum:<FPP enum>"; selector variant as Feature[Entry]
F = [
 # link
 ("DeviceLinkThroughputLimit", "THROUGHPUT_LIMIT", "U32", "bytes/s", "500000000", "T"),
 ("DeviceLinkBandwidthReserve", "BANDWIDTH_RESERVE", "F64", "percent", None, "T"),
 ("DeviceIndicatorMode", "INDICATOR_MODE", "enum:IndicatorMode", "", None, "T"),
 # image format
 ("PixelFormat", "PIXEL_FORMAT", "enum:PixelFormat", "", "PixelFormat.BayerRG8", "T"),
 ("AdcBitDepth", "ADC_BIT_DEPTH", "enum:AdcBitDepth", "", None, "T"),
 ("IspEnable", "ISP_ENABLE", "bool", "", None, "T"),
 ("BinningHorizontalMode", "BINNING_HORIZONTAL_MODE", "enum:BinningMode", "", None, "T"),
 ("BinningVerticalMode", "BINNING_VERTICAL_MODE", "enum:BinningMode", "", None, "T"),
 ("BinningHorizontal", "BINNING_HORIZONTAL", "U32", "", None, "T"),
 ("BinningVertical", "BINNING_VERTICAL", "U32", "", None, "T"),
 ("DecimationHorizontal", "DECIMATION_HORIZONTAL", "U32", "", None, "T"),
 ("DecimationVertical", "DECIMATION_VERTICAL", "U32", "", None, "T"),
 ("Width", "WIDTH", "U32", "pixels", None, "T"),
 ("Height", "HEIGHT", "U32", "pixels", None, "T"),
 ("OffsetX", "OFFSET_X", "U32", "pixels", None, "T"),
 ("OffsetY", "OFFSET_Y", "U32", "pixels", None, "T"),
 ("ReverseX", "REVERSE_X", "bool", "", None, "T"),
 ("ReverseY", "REVERSE_Y", "bool", "", None, "T"),
 ("TestPattern", "TEST_PATTERN", "enum:TestPattern", "", None, "T"),
 # exposure
 ("ExposureMode", "EXPOSURE_MODE", "enum:ExposureMode", "", None, "T"),
 ("SensorShutterMode", "SHUTTER_MODE", "enum:ShutterMode", "", None, "T"),
 ("ExposureAuto", "EXPOSURE_AUTO", "enum:AutoMode", "", "AutoMode.Off", "T"),
 ("ExposureTime", "EXPOSURE_TIME", "F64", "us", "1000.0", "T"),
 # trigger
 ("TriggerMode", "TRIGGER_MODE", "enum:TriggerMode", "", None, "T"),
 ("TriggerSource", "TRIGGER_SOURCE", "enum:TriggerSource", "", None, "T"),
 ("TriggerActivation", "TRIGGER_ACTIVATION", "enum:TriggerActivation", "", None, "T"),
 ("TriggerOverlap", "TRIGGER_OVERLAP", "enum:TriggerOverlap", "", None, "T"),
 ("TriggerDelay", "TRIGGER_DELAY", "F64", "us", None, "T"),
 # analog
 ("GainAuto", "GAIN_AUTO", "enum:AutoMode", "", "AutoMode.Off", "T"),
 ("Gain", "GAIN", "F64", "dB", "0.0", "T"),
 ("BlackLevel", "BLACK_LEVEL", "F64", "percent", None, "T"),
 ("BlackLevelClampingEnable", "BLACK_LEVEL_CLAMPING_ENABLE", "bool", "", None, "T"),
 ("GammaEnable", "GAMMA_ENABLE", "bool", "", None, "T"),
 ("Gamma", "GAMMA", "F64", "", None, "T"),
 ("BalanceWhiteAuto", "BALANCE_WHITE_AUTO", "enum:AutoMode", "", None, "T"),
 ("BalanceRatio[Red]", "BALANCE_RATIO_RED", "F64", "", None, "T"),
 ("BalanceRatio[Blue]", "BALANCE_RATIO_BLUE", "F64", "", None, "T"),
 # automatic exposure and white balance
 ("AutoExposureTargetGreyValueAuto", "AUTO_EXPOSURE_TARGET_GREY_VALUE_AUTO", "enum:TargetGreyValueAuto", "", None, "R"),
 ("AutoExposureTargetGreyValue", "AUTO_EXPOSURE_TARGET_GREY_VALUE", "F64", "percent", None, "R"),
 ("AutoExposureLightingMode", "AUTO_EXPOSURE_LIGHTING_MODE", "enum:LightingMode", "", None, "R"),
 ("AutoExposureMeteringMode", "AUTO_EXPOSURE_METERING_MODE", "enum:MeteringMode", "", None, "R"),
 ("AutoExposureExposureTimeLowerLimit", "AUTO_EXPOSURE_EXPOSURE_TIME_LOWER_LIMIT", "F64", "us", None, "R"),
 ("AutoExposureExposureTimeUpperLimit", "AUTO_EXPOSURE_EXPOSURE_TIME_UPPER_LIMIT", "F64", "us", None, "R"),
 ("AutoExposureGainLowerLimit", "AUTO_EXPOSURE_GAIN_LOWER_LIMIT", "F64", "dB", None, "R"),
 ("AutoExposureGainUpperLimit", "AUTO_EXPOSURE_GAIN_UPPER_LIMIT", "F64", "dB", None, "R"),
 ("AutoExposureGreyValueLowerLimit", "AUTO_EXPOSURE_GREY_VALUE_LOWER_LIMIT", "F64", "percent", None, "R"),
 ("AutoExposureGreyValueUpperLimit", "AUTO_EXPOSURE_GREY_VALUE_UPPER_LIMIT", "F64", "percent", None, "R"),
 ("AutoExposureEVCompensation", "AUTO_EXPOSURE_EV_COMPENSATION", "F64", "", None, "R"),
 ("AutoExposureControlLoopDamping", "AUTO_EXPOSURE_CONTROL_LOOP_DAMPING", "F64", "", None, "R"),
 ("AutoExposureControlPriority", "AUTO_EXPOSURE_CONTROL_PRIORITY", "enum:ControlPriority", "", None, "R"),
 ("AasRoiEnable[Ae]", "AAS_ROI_ENABLE", "bool", "", None, "R"),
 ("AasRoiOffsetX[Ae]", "AAS_ROI_OFFSET_X", "U32", "pixels", None, "R"),
 ("AasRoiOffsetY[Ae]", "AAS_ROI_OFFSET_Y", "U32", "pixels", None, "R"),
 ("AasRoiWidth[Ae]", "AAS_ROI_WIDTH", "U32", "pixels", None, "R"),
 ("AasRoiHeight[Ae]", "AAS_ROI_HEIGHT", "U32", "pixels", None, "R"),
 ("BalanceWhiteAutoProfile", "BALANCE_WHITE_AUTO_PROFILE", "enum:WhiteBalanceProfile", "", None, "R"),
 ("BalanceWhiteAutoDamping", "BALANCE_WHITE_AUTO_DAMPING", "F64", "", None, "R"),
 ("BalanceWhiteAutoLowerLimit", "BALANCE_WHITE_AUTO_LOWER_LIMIT", "F64", "", None, "R"),
 ("BalanceWhiteAutoUpperLimit", "BALANCE_WHITE_AUTO_UPPER_LIMIT", "F64", "", None, "R"),
 ("ExposureActiveMode", "EXPOSURE_ACTIVE_MODE", "enum:ExposureActiveMode", "", None, "R"),
 # on-camera image processing
 ("ColorTransformationEnable", "COLOR_TRANSFORMATION_ENABLE", "bool", "", None, "R"),
 ("RgbTransformLightSource", "RGB_TRANSFORM_LIGHT_SOURCE", "enum:LightSource", "", None, "R"),
 ("SaturationEnable", "SATURATION_ENABLE", "bool", "", None, "R"),
 ("Saturation", "SATURATION", "F64", "", None, "R"),
 ("SharpeningEnable", "SHARPENING_ENABLE", "bool", "", None, "R"),
 ("SharpeningAuto", "SHARPENING_AUTO", "bool", "", None, "R"),
 ("Sharpening", "SHARPENING", "F64", "", None, "R"),
 ("SharpeningThreshold", "SHARPENING_THRESHOLD", "F64", "", None, "R"),
 ("DefectCorrectStaticEnable", "DEFECT_CORRECT_STATIC_ENABLE", "bool", "", None, "R"),
 ("DefectCorrectionMode", "DEFECT_CORRECTION_MODE", "enum:DefectCorrectionMode", "", None, "R"),
 # rate: written last, its limits depend on everything above
 ("AcquisitionFrameRateEnable", "FRAME_RATE_ENABLE", "bool", "", "true", "T"),
 ("AcquisitionFrameRate", "FRAME_RATE", "F64", "Hz", "1.0", "T"),
]

# enumerations: FPP enum -> (table feature to take the entries from, or a literal list [R])
E = {
 "PixelFormat": "PixelFormat", "AdcBitDepth": "AdcBitDepth", "BinningMode": "BinningHorizontalMode",
 "TestPattern": "TestPattern", "ExposureMode": "ExposureMode", "ShutterMode": "SensorShutterMode",
 "AutoMode": "ExposureAuto", "TriggerMode": "TriggerMode", "TriggerSource": "TriggerSource",
 "TriggerActivation": "TriggerActivation", "TriggerOverlap": "TriggerOverlap", "IndicatorMode": "DeviceIndicatorMode",
 "TargetGreyValueAuto": ["Off", "Continuous"],
 "LightingMode": ["AutoDetect", "Backlight", "Frontlight", "Normal"],
 "MeteringMode": ["Average", "Spot", "Partial", "CenterWeighted", "HistgramPeak"],
 "ControlPriority": ["Gain", "ExposureTime"],
 "WhiteBalanceProfile": ["Indoor", "Outdoor"],
 "ExposureActiveMode": ["Line1", "AnyPixels", "EveryPixel"],
 "LightSource": ["General", "Tungsten2800K", "WarmFluorescent3000K", "CoolFluorescent4000K", "Daylight5000K",
                 "Cloudy6500K", "Shade8000K", "Custom"],
 "DefectCorrectionMode": ["Average", "Highlight", "Zero"],
}
ENUM_DOC = {"AutoMode": "Automatic mode of exposure, gain or white balance",
            "BinningMode": "How binned pixels are combined"}

def main(table, out):
    feats = {f["name"]: f for f in json.load(open(table))["features"]}
    src = json.load(open(table))["source"]
    lines = ["# GENERATED by docs/design-record/gen_model.py from the register table of device description",
             "# sha256 %s - do not edit." % src["sha256"],
             "# The enumerator names are the entry names of the device description. The numeric value of an",
             "# enumerator is its position, not the register value: the static register table maps one to the other.",
             "", "module Blackfly {", ""]
    for name, s in E.items():
        if isinstance(s, str):
            ents, mark = [e["name"] for e in feats[s]["entries"]], "entries of `%s`" % s
        else:
            ents, mark = s, "entries from the vendor's model reference; confirmed when the table is regenerated"
        lines += ["  @ %s (%s)" % (ENUM_DOC.get(name, "Camera feature enumeration"), mark), "  enum %s {" % name]
        lines += ["    %s" % e for e in ents] + ["  }", ""]
    lines += ["}"]
    open(out + "/CameraFeatureTypes.fpp", "w").write("\n".join(lines) + "\n")

    prm, tlm, md = [], [], []
    hdr = "# GENERATED by docs/design-record/gen_model.py - do not edit.\n"
    for i, (feat, p, t, unit, dflt, s) in enumerate(F):
        base = re.sub(r"\[.*\]", "", feat)
        if s == "T":
            assert base in feats, feat
            assert feats[base]["access"] == "RW", feat
        fpp = t.split(":")[1] if t.startswith("enum:") else t
        chan = re.sub(r"\[(\w+)\]", lambda m: "" if m.group(1) == "Ae" else m.group(1), feat)
        u = (" (%s)" % unit) if unit else ""
        d = (" default %s" % dflt) if dflt else ""
        prm += ["@ Camera feature `%s`%s" % (feat, u), "param %s: %s%s id %d" % (p, fpp, d, i), ""]
        tlm += ["@ Read-back of camera feature `%s`%s" % (feat, u), "telemetry %s: %s id %d update on change" % (chan, fpp, 0x40 + i), ""]
        md.append("| `%s` | `%s` | `%s` | %s | %s | `%s` | %s |" % (p, feat, fpp, unit or "—", dflt.split(".")[-1] if dflt else "—", chan,
                                                                   "register table" if s == "T" else "Table 1 name"))
    open(out + "/CameraParams.fppi", "w").write(hdr + "\n" + "\n".join(prm))
    open(out + "/CameraReadBack.fppi", "w").write(hdr + "\n" + "\n".join(tlm))
    open(out + "/docs/design-record/parameter-table.md", "w").write("\n".join(md) + "\n")
    print(len(F), "features;", sum(1 for f in F if f[5] == "T"), "in the register table;", len(E), "enumerations")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
