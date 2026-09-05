# Extracted-game RWS corpus

Corpus root inspected: `C:\Users\eon\Desktop\Panzers`, 2026-09-05.

## Inventory

The archive contains 1,299 files, including 36 `.rws` files totaling 35,221,739
bytes. All 36 load as bounded RenderWare streams. Only the already-known ST05 map
and collision files produce diagnostics.

| Root chunk | Vendor/object | Files | Bytes | Library stamp |
|---|---|---:|---:|---|
| World (`0x0000000B`) | Core / `0x0B` | 1 | 6,187,753 | RW 3.7.0.2 build 55 |
| Clump (`0x00000010`) | Core / `0x10` | 1 | 28,979,398 | RW 3.7.0.2 build 55 |
| `0x00000907` | RenderWare Physics / `0x07` | 33 | 48,300 | RW 3.7.0.2 build 24 |
| `0x00000909` | RenderWare Physics / `0x09` | 1 | 6,288 | RW 3.7.0.2 build 24 |

The vendor assignment is confirmed by Criterion's `RwPluginVendor` enum:
`rwVENDORID_CRITERIONRWP = 0x000009`. Therefore `0x907` and `0x909` are not Pyro
chunk IDs: they are objects 7 and 9 from the official RenderWare Physics component.

## Physics streams

Every Physics root contains exactly one standard Struct child spanning the root
payload. Object `0x07` files occur beside matching `.rpc` render models under
`Models/Deco`, `Models/Vehi`, and `Models/Weap`. Payloads are 240–5,384 bytes and
contain repeated tagged scalars, vectors, quaternions/matrices, and apparent shape
records. This strongly indicates collision/rigid-body descriptions associated with
the render models.

The sole object `0x09` is `Models/ragdoll.rws`. It contains recognizable bone names
such as `bone20`, which independently supports a Physics ragdoll/constraint role.

Ghidra analysis of `CommXPC.exe` resolves the root types conclusively. The executable
statically contains RenderWare Physics 3.7 and source-identification strings for
`RwpBodyDef.c`, `RwpRagdollDef.c`, `RwpGenericDef.c`, `RwpVolume.c`, `RwpBox.c`,
`RwpSphere.c`, `RwpCapsule.c`, `RwpCylinder.c`, and `RwpTrilist.c`.

The game's resource dispatcher at `0x006B8CD0` reads the root header and dispatches:

| Root | Reader | Resolved type |
|---:|---:|---|
| `0x907` | `0x00822B20` | `RwpBodyDef` |
| `0x909` | `0x00823B70` | `RwpRagdollDef` |
| `0x90B` | `0x00823270` | `RwpGenericDef` (supported by the game; absent from this corpus) |

The embedded serializer uses self-describing 32-bit tags. Confirmed primitive tags
are `0x01` (u16), `0x03` (u32), `0x04` (u32 array), `0x05` (float), `0x06`
(three-float vector), `0x07` (four-float quaternion), `0x08` (12-float 3x4 matrix),
and `0x09` (position/quaternion transform). A tag's upper 16 bits carry a record
version where applicable, for example `0x00010017`.

`RwpBodyDef` begins with tag `0x18` and embeds a versioned `0x17` volume record.
Volume tag `0x0B` selects its shape. Shapes observed and validated in the corpus are
sphere (`0x0E`), capsule (`0x0F`), box (`0x10`), cylinder (`0x11`), and Trilist
aggregate (`0x13`). Trilist records recursively contain other volume records. Common
volume data comprises a 3x4 matrix, a shape-size/fatness float, two still-conservative
material coefficients, and (in version 1) group and flag u16 values. The first float
occupies volume offset `+0x4C`; it is the radius in a sphere and capsule, but is not a
generic radius for every shape.

The body fields are not a world transform. Decompiled mass-property calculation at
`0x00801550`, backed by the volume calculation at `0x007FEB40`, establishes this
layout:

| Body offset | Stream field | Meaning |
|---:|---|---|
| `+0x04` | final vec3 | center of mass |
| `+0x10` | first float | mass (a negative setter input requests density-to-mass conversion) |
| `+0x14` | transform vec3 | principal inertia values |
| `+0x20` | transform quaternion | principal-inertia orientation |
| `+0x30` | following float | scalar/spherical inertia approximation |
| `+0x34`, `+0x38` | following floats | unresolved; both default to `0.01` |
| `+0x3C` | following vec3 | unresolved |
| `+0x48` | u32 | body flags; bit 1 selects full principal-inertia data |

These names are derived from data flow rather than guessed from field values: the
calculation obtains volume, center, and inertia from the shape, multiplies inertia
by mass, stores the center, and selects scalar versus oriented principal inertia.

All 33 `0x907` files consume exactly with this grammar. The sole `0x909` file also
consumes exactly: it contains 11 embedded body definitions, 10 joint records, ten
joint-pair entries, a lookup table, and 11 body IDs.

Evidence-backed names were saved into the Ghidra project for the dispatcher and the
key readers at `0x00822B20`, `0x00823B70`, `0x00823270`, `0x00829BF0`, `0x008239A0`,
`0x008230A0`, `0x00829950`, `0x00828C10`, `0x00829350`, and `0x00829F70`. Supporting
initializers and mass-property routines are also annotated there.

## RenderWare Studio 2.0.1 source cross-check

The local Studio repository was searched for the Physics object IDs, body-definition
symbols, ragdoll serializers, and Physics stream callbacks. It confirms the vendor
assignment and contains a Workspace icon registration for `rwpID_BODYDEF`, but the
RenderWare Physics component implementation and serialization headers are absent;
the executable itself supplied the missing implementation evidence.
Studio's changelog also refers to Karma physics integration, without including the
removed implementation.

## Pyro Studios object metadata (`0xFFFFFF00`)

Ghidra resolves the formerly unknown extension as one Pyro plugin ID registered on
multiple RenderWare classes by `CSF_AttachPyroObjectMetadataPlugins` at `0x006C23B0`.
It is not a single payload schema. Its owning-object distribution is:

| Owner | Count | Observed version / size |
|---|---:|---|
| Atomic (`0x14`) | 369 | version 11; mainly 88 or 112 bytes |
| Material (`0x07`) | 772 | version 2; mainly 32 bytes, longer when named |
| Frame (`0x0E`) | 622 | version 1; 8 bytes |
| World Sector (`0x09`) | 1 | version 1; 12 bytes |
| Light (`0x12`) | 1 | version 2; 8 bytes |

The matching stream readers are `0x006C1BB0` (Atomic), `0x006C1FD0` (Material),
`0x006C2130` (Frame), `0x006BF6F0` (World Sector), and `0x006BF7F0` (Light).
`0x006C0FA0` proves strings use a `u32` byte length followed by exactly that many
bytes; the terminating null exists only in the runtime allocation. The decoder now
selects the schema from the enclosing object, consumes all 1,765 corpus instances,
and exposes unknown numeric fields without assigning speculative gameplay names.

Material value correlation identifies record word 3 as a flags/mask field and word
4 as a surface-type ID. Named examples include `1=Tierra`, `2=Piedra`, `3=Metal`,
`5=Vegetacion`, `6=Madera`, `7=Cristal_opaco`, `9=Barro`, `10=Baldosa`,
`11=Cemento`, `12=Escaleras`, and `18=escaleras_piedra`. The names strongly support
a collision/audio/gameplay surface classification rather than rendering state.

Of the 369 Atomic records, 204 carry the optional six-float bounds and four carry a
non-empty first string: `ARBOL_3`, `BANDERA_A`, `BANDERA_B`, and `CABLES`. The second
Atomic string is empty in this corpus. This supports treating the first string as an
object name while retaining the second as an unresolved string slot.

`Studio/Examples/Models/Warrior/ragdolls.zip` contains two DFF files named for local
and world ragdoll coordinates. Inspection shows that they are ordinary Clumps with
Geometry, Skin, and HAnim plugins; neither contains a Physics `0x907` or `0x909`
object. They served as an independent Skin decoder fixture, but are not evidence for
the Physics payload grammar. This negative result prevents accidentally transferring
Graphics Skin semantics onto the unrelated Physics ragdoll stream.

## Reproducible scan

```powershell
.\build\Release\rws-corpus.exe C:\Users\eon\Desktop\Panzers
```

The scanner reports every RWS path, size, root format, decoded library/build stamp,
diagnostics, root-format totals, and recursive chunk/plugin counts. It is intended
for comparing additional stages without hardcoding ST05 paths.
