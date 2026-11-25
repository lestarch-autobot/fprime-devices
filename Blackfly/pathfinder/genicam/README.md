# GenICam description and static register table

## Files

| File | Content |
| --- | --- |
| `genicam-file-01908168.zip`, `genicam-file-017F4EC3.zip` (not committed; pull with `u3vprobe xml`) | Manifest entry 0 exactly as downloaded from each camera with `u3vprobe xml` (address 0x60000000, 67623 bytes). |
| `public_camxml.xml` (not committed) | The one file inside the zip (876103 bytes, dated 2021-03-29 in the zip). |
| `gen_regtable.py` | Design-time generator, Python 3 standard library only. |
| `blackfly_regtable.json` | Generated table, machine readable, complete (109 features). |
| `BlackflyRegTable.h` | Generated C header with the same table (compiles as C11 and C++). |
| `regmap.tsv` | Generated flat name/address map used by `u3vprobe -m` (`get NAME`, `set NAME VALUE`). |

## Identity

| | 01908168 | 017F4EC3 |
| --- | --- | --- |
| zip SHA-256 | `78df18f24a34e5a94f0dac4540db57c530439812103b5a2bc3f4132d210d5180` | same |
| zip SHA-1 (also in the manifest entry) | `ad4370397be7c60b6c00af21b3baa24054daebf1` | same |
| XML SHA-256 | `9be43bbdcd76435a93e9dce4dd64e3535f0889b56e96095d0007616f98ffd4ad` | same |

The two cameras deliver byte-identical files. XML header: ModelName `BFS_U3_244S8`, VendorName `FLIR`, version 1.0.0, GenApi schema 1.1,
ProductGuid `EE4B7E09-DA29-4956-A1EC-56226E31CF9C`, VersionGuid `85400240-452F-43af-8CDD-4B721C1D261A`. The generated table records the XML hash, so a
backend can refuse a camera whose manifest SHA-1 differs.

## Regenerating

```sh
cd genicam
python3 gen_regtable.py -o . public_camxml.xml          # the flight feature list (default)
python3 gen_regtable.py -o /tmp/all --all public_camxml.xml   # every feature in the XML, for exploration
```

The default feature list is at the top of `gen_regtable.py`. `--all` reports the features it cannot express as one register.

## How the XML is flattened

Every feature in this XML follows one layout: a block of 4-byte little-endian registers on port `Device`.

| Offset | Content |
| --- | --- |
| +0x00 | flags: bit 31 implemented, bit 30 available, bit 28 locked |
| +0x04 | value (enumerations: +0x08) |
| +0x08 | minimum |
| +0x0C | maximum |
| +0x10 | increment |
| +0x14 | number of fraction bits (floats) |

The generator follows each feature's `pValue` until it reaches a register node (`IntReg`, `MaskedIntReg`, `StringReg`, `Register`) and records
address, length, access, sign, endianness and bit range. `pMin`, `pMax`, `pInc`, `pIsImplemented`, `pIsAvailable` and `pIsLocked` are followed the same way.
`Converter`, `IntConverter`, `SwissKnife` and `IntSwissKnife` nodes on the way are evaluated numerically at four sample values and classified:

| Kind | Meaning | Features |
| --- | --- | --- |
| `identity` | value = raw | 83 features: all integers, enumerations, booleans, commands, strings; `ExposureTime` (us), `Width`, `PayloadSize`, ... |
| `linear` | value = raw x scale | `Gain` (dB = raw x 6.020599913 / 2^14) |
| `fixed_point` | value = raw x scale / 2^fracbits, fracbits read from a register | `DeviceTemperature`, `PowerSupplyVoltage`, `PowerSupplyCurrent`, `BlackLevel` (scale 100, percent), `Gamma`, `BalanceRatio`, `DeviceLinkBandwidthReserve` (scale 100) |
| `reciprocal` | value = numerator register / raw | `AcquisitionFrameRate`, `AcquisitionResultingFrameRate` (raw is the frame period in ticks) |
| `formula` | left as text | only with `--all` |

Not a register at all:

- `PixelSize` is computed from the bits-per-pixel field of `PixelFormat`. It is emitted with conversion `formula` and no address; a manager does not need it (`PayloadSize` is a register).
- Most selectors, see below.

## Selectors

Two selector mechanisms exist in this XML and both are resolved at generation time.

1. Host-side selectors (JSON `host_side_selector`, or a constant with one entry): `TestPatternGeneratorSelector`, `TriggerSelector`, `BinningSelector`,
   `DecimationSelector`, `BlackLevelSelector`, `BalanceRatioSelector`, `EventSelector`; single-entry `DeviceTemperatureSelector`, `DevicePowerSupplySelector`, `GainSelector`.
   The selector is not a device register; it only changes the address used by the selected feature. The table lists one address per selector entry
   (`variants` in JSON, `BFLY_VARIANTS` in the header, `Name[Entry]` in `regmap.tsv`). The plain name is the XML default entry.
   Example: `TestPattern` is 0xF0004208 for `Sensor` and 0xF0004228 for `PipelineStart`.
2. Register selectors: `ChunkSelector` (0x150048) and `UserSetSelector` (0x74008) are real registers that must be written before the selected feature
   (`ChunkEnable`, `UserSetLoad`) is accessed. The table names the selector in `selected_by` / `selector`.

## What hardware said about the table

- All 109 default features were read on both cameras with `u3vprobe get` (see `../truth/regsnap-*.txt`).
- Numeric conversions were checked against hardware for `Gain`, `BlackLevel`, `Gamma`, `ExposureTime`, `AcquisitionFrameRate` and `AcquisitionResultingFrameRate` (`../truth/05-readback-*`, `06-rate-*`).
- The XML lists more enumeration entries than a given selector variant accepts (`TestPattern`): the per-entry implemented/available bits in the table are authoritative, and the camera answers 0x8002 otherwise.
- With `--all`, some XML nodes answer INVALID_ADDRESS or INVALID_PARAMETER on this firmware. Only the default list is claimed to be verified.
