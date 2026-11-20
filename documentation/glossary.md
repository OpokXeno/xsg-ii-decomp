# Glossary

This glossary explains the terms used across the pages of the Xenosaga Episode II architecture documentation. Each entry gives a short definition and links to the place where the term is described in full.

[← Back to the overview](README.md)

[Symbols](#symbols-and-numbers) · [A](#a) · [B](#b) · [C](#c) · [D](#d) · [E](#e) · [F](#f) · [G](#g) · [H](#h) · [I](#i) · [J](#j) · [K](#k) · [L](#l) · [M](#m) · [N](#n) · [O](#o) · [P](#p) · [Q](#q) · [R](#r) · [S](#s) · [T](#t) · [U](#u) · [V](#v) · [W](#w) · [X](#x) · [Y](#y) · [Z](#z)

## Symbols and numbers

**12.4 fixed point**: The GS screen coordinate format: pixel position times 16. See [Math: Screen coordinates](libraries/math.md#screen-coordinates).

**16m.bin**: 16 MiB of zeros, first record and last file of each story group. See [Groups and pieces](formats/scripts-and-archives.md#groups-and-pieces). See also [16m.bin](formats/game-tables.md#16mbin).

**8.8 fixed point (cfe)**: The integer form of key values passed by scripts, value = integer / 256. See [Cutscenes: The key node](scripting/cutscenes.md#the-key-node).

## A

**A+D pair**: A 64-bit GS register value followed by the register number (`GsAdPair`). Nearly all 2D drawing is lists of such pairs. See [Rendering: Writing into a packet](architecture/rendering.md#writing-into-a-packet).

**Accent codes**: the EUC hiragana codes `A4 A1` to `A4 A5`, which the Western font draws as ä é ê Ä ®. See [Text and fonts: Two-byte characters](formats/text-and-fonts.md#two-byte-characters).

**Accessory record (`EsAccessoryRecord`)**: E.S. equipment of box 2. Grants skill record 64 plus its number. See [Accessories](formats/game-tables.md#accessories).

**`ACT_updateEnemy`**: Per-frame update of every record of kind 1: frozen test, action function, sight, contact, turning. See [Field enemies: The per-frame update](game/field-enemies.md#the-per-frame-update).

**Action (`state->action`)**: What a character does right now (0 to 13). `Enemy_ActionReady` starts one, `ACT_updateEnemy` runs its function each frame. See [Field enemies: The actions](game/field-enemies.md#the-actions).

**Action key and move key**: The effect key pairs a unit uses for its action effect and move effect (`unitActionKey`, `unitMoveKey`). See [The battle engine: Effect keys of a unit](game/battle-engine.md#effect-keys-of-a-unit).

**Action stock**: `struct ActStock`, the queue of the current round (four turn slots and two Boost slots). It is the turn window of the HUD and is not the same as Stock. See [Battle: Filling the action stock](game/battle.md#filling-the-action-stock).

**Active table (`XenoActorActiveTable`)**: 48 pointers placed after the pool, one per actor in use. The update and draw passes walk it. See [Actors: The pool and the matrix heap](libraries/actors.md#the-pool-and-the-matrix-heap).

**Active, default and applied light set**: Three `XglLightSet` blocks of `src/main/sef_light_set.c` shared by effects and battle units. See [sef: The three light sets](libraries/sef.md#the-three-light-sets).

**Actor (`XenoActor`)**: The engine's record for one animated or rigid 3D object, `0x7F0` bytes: transform, resource set, motion package, matrices, light set, collision probe and draw switches. See [Actors: What an actor is](libraries/actors.md#what-an-actor-is).

**Actor entry (`CfeActor`)**: One of 64 records that drive a `XenoActor` in a cutscene. See [Cutscenes: Actors](scripting/cutscenes.md#actors).

**Actor entry table (`sefEntryTable`)**: Per-frame cache of attach point matrices and positions for up to 24 actors, with entry flags and effect lights. See [sef: The actor entry table](libraries/sef.md#the-actor-entry-table).

**Actor history**: Fire type 13: a wide orange ribbon spiral along a curve. See [Battle effects: Actor history (type 13)](game/battle-effects.md#actor-history-type-13).

**Actor key**: Table index of a model, texture and skeleton triple in `titlebg2.bin`. `0xFA` is an alias of index 19. See [Boot and title: The demo scene](game/title-and-boot.md#the-demo-scene).

**Actor matrix (`localMatrix`)**: The world matrix of an actor built by `XenoActorPushMatrix`. See [Actors: The actor matrix](libraries/actors.md#the-actor-matrix).

**Actor matrix heap (RG heap 5)**: The `0x38000`-byte heap that holds the joint matrix buffers of all actors. See [Actors: The pool and the matrix heap](libraries/actors.md#the-pool-and-the-matrix-heap).

**Actor path**: Fire type 12: a fast purple swirl head along a curve. See [Battle effects: Actor path (type 12)](game/battle-effects.md#actor-path-type-12).

**Actor pool**: 48 `XenoActor` records of `0x7F0` bytes in a static buffer, re-created by `XenoActorPoolInitDefault`. See [Memory: Fixed pools](architecture/memory.md#fixed-pools).

**Actor pool (`XenoActorPool`)**: The static array of 48 actor records in `xenoActorPoolBuffer`. A record with `attr` 0 is free. See [Actors: The pool and the matrix heap](libraries/actors.md#the-pool-and-the-matrix-heap).

**ADC restart**: The clipping rule of all MEf strips: after a clipped vertex the next vertices carry the ADC bit so the strip restarts without drawing. See [Battle effects: GS packets](game/battle-effects.md#gs-packets).

**ADC shift register**: The integer register in which a loop collects strip start, clip and face flags, doubles it per vertex and writes it into lane w, so the right triangles get the `ADC` bit. See [pxy: The geometry pass](libraries/pxy.md#the-geometry-pass).

**ADPCM flags**: The second byte of an ADPCM block: bit 0 end, bit 1 repeat, bit 2 loop start. They make a sample loop or stop inside the chip, so a bank needs no loop table. See [Sound files: SPU2 ADPCM](formats/sound.md#spu2-adpcm).

**ADSR words**: The two SPU2 envelope register values of a layer. The default pair is `0x808F`, `0xDFE6`. See [Sound files: Keys](formats/sound.md#keys).

**ADX**: CRI's stream format: big-endian header, 18-byte frames of 32 samples per channel, end mark `80 01`. See [Sound files: ADX streams](formats/sound.md#adx-streams).

**ADX loop fields**: Loop start and end as sample and byte positions in the header. Only six files have them. See [Sound files: Loops](formats/sound.md#loops).

**ADXF**: CRI file layer. The game opens the whole disc as `CDV:DVD-ROM` and reads sectors with `ADXF_Seek`/`ADXF_ReadNw`. See [Runtime libraries: The ADX file system](architecture/runtime-libraries.md#the-adx-file-system).

**ADXT**: CRI ADX stream player. The game keeps two players. See [Runtime libraries: The ADX player](architecture/runtime-libraries.md#the-adx-player).

**Aim target / focus target**: A world position that one joint (`targetNode`, default 0x1D) turns towards, with limits of 0.9 and 0.4 radians and smoothing. See [Animation: Aim control](libraries/animation.md#aim-control).

**Alert effect**: Effect `0x10000021` kept in `gameWork.releaseHandle` while any enemy is in action 4 or 5, removed by `Check_EnemyFound`. See [Field enemies: Discovery](game/field-enemies.md#discovery).

**AlignedVector4**: A `Vector4` aligned to 16 bytes so it can be moved with one 128-bit load and store. See [Math: Types](libraries/math.md#types).

**Alpha plane**: The one-byte-per-pixel plane behind the JPEG stream of a backdrop, at the JPEG size rounded up to 64. See the same section.

**Alter**: A ribbon or line trail that samples actor parts every frame (`SdvAlter`, 49 records). See [sdv: Alters](libraries/sdv.md#alters).

**Alter event**: One row of the alter sequence (`SdvAlterEvent`, 28 halfwords). See [sdv: Alter events](libraries/sdv.md#alter-events).

**Alter row**: The 28-word `SdvAlterEvent` that starts one ribbon trail. See [Effect data files: alter rows](formats/effects.md#alter-rows).

**Ambient fader**: The 20-frame fade of map and ambient colour in `sdvExecAmbient`. It is inert in this game. See [sdv: Ambient light](libraries/sdv.md#ambient-light).

**ANALYZE panel**: 256 by 192 window with the stats of an analysed enemy (`analyzePanelStart`). See [Fonts and text: Entries, choice menus and the ANALYZE panel](libraries/fonts-and-text.md#entries-choice-menus-and-the-analyze-panel).

**Anchor**: The record, joint or actor the look-at point follows. See [Field: CameraRange and the camera records](game/field.md#camerarange-and-the-camera-records).

**Anchor (camera)**: Bit `0x10` of a look-at key's mode. The look-at point keeps following the actor part. See [sdv: Position and look-at keys](libraries/sdv.md#position-and-look-at-keys). See also [Native functions: The camera module](scripting/natives.md#the-camera-module).

**Angle unwrapping**: `cfeChainUnwrapAngles` moves rotation keys by whole turns so neighbours are within pi. See [Cutscenes: Angle unwrapping](scripting/cutscenes.md#angle-unwrapping).

**Animation flags (`flags`)**: The per-actor animation switches (freeze, no wrap, reset blend and others). See [Actors: Animation flags](libraries/actors.md#animation-flags).

**Animation weight**: A per-motion value of a dynamics node that mixes animation (1.0) and simulation (0.0). See [Animation: The animation weight](libraries/animation.md#the-animation-weight).

**Anonymous block**: A resource block without a resource id, taken with `GameResBufferAlloc`. See [Memory: Allocating and freeing blocks](architecture/memory.md#allocating-and-freeing-blocks).

**Archive directory**: The record list at the start of `XENOSAGA.n0` (first byte = number of directory sectors) with directory records and file records, kept in work memory for the whole run. See [Files and resources: The archive directory](architecture/files-and-resources.md#the-archive-directory).

**Archive group**: One logical archive split into pieces of at most 1 GiB: `XENOSAGA.n0` holds only the directory, `.n1` and up hold the data. Group 00 is common data, group 10 is on disc 1, group 20 on disc 2. See [Files and resources: The discs](architecture/files-and-resources.md#the-discs). See also [Scripts and archives: Groups and pieces](formats/scripts-and-archives.md#groups-and-pieces).

**Archive slot (`ArcHeader`)**: One of four `0x18`-byte entries that describe a loaded directory: source, directory address, sector of the archive. Slot 0 is group 00, slot 1 the story group of the disc in the drive. See [Files and resources: Loading the directories](architecture/files-and-resources.md#loading-the-directories).

**Area**: Circle (`area`, `areaSize`) a character keeps to. A wanderer picks its targets on the rim, and a chase ends at the rim. See [Field enemies: The chase](game/field-enemies.md#the-chase).

**Arena (resource arena)**: The range recorded by the first `GameResourceWorkAlloc` call (`gameResArenaStart`/`gameResArenaEnd`). Resources inside it count as not loaded. See [Memory: Borrowing the resource area](architecture/memory.md#borrowing-the-resource-area).

**Arena and work area**: A run of resource blocks borrowed with `GameResourceWorkAlloc` for a movie, a menu or the battle. Owned blocks inside are overwritten and read again from disc by `GameResArenaRelease`. See [Files and resources: Work areas inside the resource memory](architecture/files-and-resources.md#work-areas-inside-the-resource-memory).

**Argument count**: The word a script pushes after the last argument pair, read with `VM_PLUGIN_CNT_GET`. See [The SB virtual machine: Arguments are pairs](scripting/sb-vm.md#arguments-are-pairs).

**Argument count word**: The word a script pushes after its arguments, read with `VM_PLUGIN_CNT_GET`. See [Native functions: Arguments on the stack](scripting/natives.md#arguments-on-the-stack).

**Argument jump (SB_JUMP_ARGUMENTS)**: Jump key 2, requested by a native that got a wrong argument count. Without a handler slot in the script it is fatal. See [Native functions: Argument errors](scripting/natives.md#argument-errors).

**Argument pair**: The two stack words of one native argument: the value and above it the tag. See [The SB virtual machine: Arguments are pairs](scripting/sb-vm.md#arguments-are-pairs).

**Argument slot**: The two stack words of one argument, a value word and a type word. See [Native functions: Arguments on the stack](scripting/natives.md#arguments-on-the-stack).

**ARX**: The packed file format: a 34-word header (signature `0x00585241`, unpacked size, stream size, zero flags, 30-word table) and a stream of flag bits, 3- to 9-bit table codes and 32-bit literal words. See [Files and resources: Packed files (ARX)](architecture/files-and-resources.md#packed-files-arx). See also [xgl: Timer 0, ARX and the language code](libraries/xgl.md#timer-0-arx-and-the-language-code).

**ARX stream**: The packed form of a file: 4 header words, a table of 30 frequent 32-bit words and a code stream of bit containers mixed with literal words. Used by 3 files in `yajima\`. See [Packed files (ARX)](formats/scripts-and-archives.md#packed-files-arx).

**asDispReso, asVramBase**: The six-row tables of screen sizes and VRAM pages. Only row 1 (512 x 448) is used. See [Rendering: Video memory](architecture/rendering.md#video-memory).

**Attach and detach**: `uwamonoAttachCurrent` / `uwamonoDetachCurrent`, a place change from the menu that stores the return place and position, and the way back (`umnReturn`). See [Field: Leaving and returning through the menu](game/field.md#leaving-and-returning-through-the-menu).

**Attach point**: One of the nodes 0 to 31 of an actor's motion package, cached per frame for effects. See [sef: The actor entry table](libraries/sef.md#the-actor-entry-table).

**Attach point (coordinate)**: A node number of an actor that `MSeGetActorMatrix` resolves to a world matrix. -1 is the actor origin and -2 the origin raised to height 1.5. See [Battle effects: Shared helpers of the MEf core](game/battle-effects.md#shared-helpers-of-the-mef-core).

**Attach point table (EPF)**: A `.epf` file, magic `EPF `, version 1.0 or 1.1 (`TiNodeTable`): numbered points on a skeleton with a matrix index, an offset and a rotation. See [Skeletons and motion: Attach point tables (EPF)](formats/skeletons-and-motion.md#attach-point-tables-epf).

**Attack attribute bits**: One bit order for the eight attributes in the whole file: 0x01 beam, 0x02 aura, 0x04 thunder, 0x08 fire, 0x10 ice, 0x20 pierce, 0x40 slash, 0x80 strike. See [Unit records](formats/game-tables.md#unit-records).

**Attack attributes**: The low byte of `TecRecord.actionBits`, eight element-like bits. See [Battle: An attack](game/battle.md#an-attack).

**Attack event**: Event kind 3 (`eventAttackRun`), which plays one attack and then applies its precomputed hits. See [Battle: The attack event](game/battle.md#the-attack-event).

**Attack level**: `UwamonoRecord.level` 0 to 2 of a target, selecting actor mode 3 to 5 and the timing table entry. See [Field: Attacking map objects](game/field.md#attacking-map-objects).

**Attack table**: The table of legal (technique slot, target) pairs that `atkTblBuild` writes into the script's globals. See [Battle: Enemy thinking](game/battle.md#enemy-thinking).

**Attack table (atkTbl)**: A table in the battle script's globals that `bat.atkTbl` fills with (technique slot, target unit) rows. See [Native functions: Enemy thinking and commands](scripting/natives.md#enemy-thinking-and-commands).

**Attribute (collision kind)**: The 32-bit meaning of a triangle: ground, wall class, skip, jump area, elevator, slope, territory, material. See [Maps and units: Triangles and attributes](game/map-and-units.md#triangles-and-attributes).

**Attribute area**: The upper half of a packet buffer, from `limit` upward, where data referenced by `REF` tags is copied (`nmlPacketSetAttributeData*`). See [Rendering: Packets](architecture/rendering.md#packets).

**Attribute code**: The 32-bit value a query returns for a triangle. Its high byte tells the family. See the same section.

**Attribute data**: Per-frame data (matrices, vectors, register queues) placed in the upper half of a packet buffer through `XglPacket.cursor`. See [Memory: Packet buffers as frame memory](architecture/memory.md#packet-buffers-as-frame-memory).

**Attribute reaction**: `StatBlock.slotValue` and `slotRoll`, the weakness, resistance, immunity or absorption of a unit per attribute, in twentieths. See [Battle: An attack](game/battle.md#an-attack).

**Attribute table (`kindTable`)**: The list of 32-bit attribute codes of a file, copied behind every mesh entry. A triangle's `kind` is an index into it. See [Maps and field data: The attribute table](formats/maps-and-field-data.md#the-attribute-table).

**avitest.ipu**: Leftover file `iwase\avitest.ipu` without the `ipum` header, used by no code. See [section 4.7](formats/movies-images-and-icons.md#47-avitestipu).

## B

**Back attack**: An attack from behind the target (`unitIsInFacingArc`). It cannot be evaded or guarded and adds 50 to the critical chance. See [Battle: Back attacks](game/battle.md#back-attacks).

**Backdrop**: A JPEG image used instead of a map texture, drawn in four strips by `mapBackdropDraw`. See [Field: The map model and the map objects](game/field.md#the-map-model-and-the-map-objects).

**Background layers**: the three model layers (back, middle, front) behind the menus, managed by `bg_layer.c`. See [Screen effects: Background layers](libraries/screen-effects.md#background-layers). See also [Main menu: The background layers](game/main-menu.md#the-background-layers).

**Background layers (`BG_BACK`, `BG_MIDDLE`, `BG_FRONT`)**: Three actors that form the moving background of the menu screens. See [Actors: Background layers of the menus](libraries/actors.md#background-layers-of-the-menus).

**Background load flag**: Bit 31 of a resource id (`RES_ID_QUEUED_LOAD`). The files are issued as queued asynchronous reads and the load may start while the library is busy. See [Files and resources: Loading by id and by name](architecture/files-and-resources.md#loading-by-id-and-by-name).

**Balance (`msgBalance`)**: The remaining debt, 10,000,000 at a new game, saved as `SaveData.balance`. See [UI windows: Tables and balance](libraries/ui-windows.md#tables-and-balance).

**Band**: An upload of 32 rows, which is one row of eight GS pages (0x100 blocks) at buffer width 512. See [Upload records](formats/models-and-textures.md#upload-records).

**Bank (KORG bank)**: A pair of files with the same name, a `.vsf` with the samples and a `.prj` with the keys and programs, loaded into one bank slot of the KORG driver. See [Sound files: How the files work together](formats/sound.md#how-the-files-work-together).

**Bank handle**: In battle, a bank slot number 3 to 8 for a unit's voice bank, with its sound memory address. See [Audio: Battle sound](architecture/audio.md#17-battle-sound).

**Bank slot**: A numbered place for a bank in the KORG driver, 0 to 23 for the game. The slot number is also the MIDI bank number of its sounds. See [Audio: Sound banks](architecture/audio.md#6-sound-banks).

**Base index**: The value in `gameResBlockTable[0].owner`. The best-fit search and `GameResourceReset` only touch entries from this index up, so blocks below it are permanent. Set by `resSelectSlot`. See [Memory: Reset and the base index](architecture/memory.md#reset-and-the-base-index).

**Base sheet**: `yosikawa\font.xtx`, texture 0, uploaded to GS block 0x3800. Holds frames, cursors, letter and digit strips and button icons. See [Fonts and text: Textures and slots](libraries/fonts-and-text.md#textures-and-slots).

**bat**: Module of 63 battle natives. See [Native functions: The bat module](scripting/natives.md#the-bat-module).

**bat.dat getters**: The functions of `src/ov01/data_record_get.c` that return records of the game tables (units, techniques, ethers, skills, items, Double Techs, experience, growth, rewards). See [Party and growth: The game tables](game/party-and-growth.md#the-game-tables).

**Batch**: One `STCYCL`, `UNPACK`..., `MSCNT` group of a mesh stream. See [The VIF stream of a mesh](formats/models-and-textures.md#the-vif-stream-of-a-mesh). See also [pxy: Batches of a part](libraries/pxy.md#batches-of-a-part).

**Batch (`PxyBatch`)**: The run of draw entries of one model. Each model has an opaque and a translucent batch. See [pxy: Batches](libraries/pxy.md#batches).

**batdat**: `yamamoto\batdat.us`, the battle data file with unit records, techniques, items, statuses and texts, read into `batDat`. See [The battle engine: The battle data getters](game/battle-engine.md#the-battle-data-getters).

**batdat.us**: The battle data file `yamamoto\batdat.us` (75,556 bytes, called `bat.dat` in source comments). 19 tables behind an offset header, loaded once into `batDat`. See [Game data tables: Battle data](formats/game-tables.md#battle-data-batdatus).

**Battle end status**: `BattleState.status` 1 to 5 (enemies gone, party gone, party left, enemies left, time up), returned by `battleRetCodeGet`. See [Battle: How a battle ends](game/battle.md#how-a-battle-ends).

**Battle heap**: The 10 MiB first-fit heap of OV01 (`ov01HeapAlloc`, `ov01HeapFree`), borrowed from the resource area for the length of a battle. See [The battle engine: The battle heap](game/battle-engine.md#the-battle-heap).

**Battle heap (OV01 heap)**: First-fit heap of `0xA00000` bytes that the battle engine borrows from the resource area. See [Memory: The battle heap](architecture/memory.md#the-battle-heap).

**Battle map**: A stage of `yamamoto\map\`, named by `dataMapNameTbl` and selected by the map number of an enemy group. See [Battle maps](formats/game-tables.md#battle-maps-in-yamamotomap).

**Battle mode (`_sefBattleMode`)**: The mode flag of the effect system. It enables the camera schedule and `MSe` objects and disables the built-in emitters. See [sdv: Start, reset and modes](libraries/sdv.md#start-reset-and-modes).

**Battle motion file**: A motion pack in `yamamoto\mot\`. Most have 51 entries, a fixed battle motion numbering, most of them empty. See [Skeletons and motion: Battle motion files](formats/skeletons-and-motion.md#battle-motion-files).

**Battle music track**: A number 1 to 6 for `battle01`, `battle02`, `boss01`, `boss02`, `bosslast`, `fanfale01` (ADX, slot 0). See [Audio: Battle sound](architecture/audio.md#17-battle-sound).

**Battle native**: A native of OV01 that targets a battle unit by index. See [Native functions: Field natives and battle natives](scripting/natives.md#field-natives-and-battle-natives).

**Battle program**: `xglMode4MicrocodeProgram`, the lit program of battle mode, Plain without point lights, clip fade and shadow styles. See [pxy: The model programs](libraries/pxy.md#the-model-programs).

**Battle request**: The fields `encounterReaction`, `encounterTarget`, `encounterFlags`, `encounterRecord` of `gameWork`. `encounterReaction != -1` makes `Game()` leave for the battle. See [Field enemies: The request](game/field-enemies.md#the-request).

**Battle result block**: `battleResult`, experience, skill points, class points and up to eight drops collected by `monsGainSet`. See [Battle: Rewards](game/battle.md#rewards).

**Battle result screen**: The screen after a battle that counts up experience and points (`battleResultScene`). See [Main menu: The battle result screen](game/main-menu.md#the-battle-result-screen).

**Battle scene**: The scene that replaces the field for a battle: `ov01SceneEnter`, `ov01SceneLoop`, `ov01SceneEnd` in `src/ov01/scene.c`. See [Battle: Setting up the scene](game/battle.md#setting-up-the-scene).

**Battle script (think script)**: The SB image `model\map\E2_xxxxx.bin` of a map, source name `thinkNNN`, linked against `bat`, `camera` and `math`. See [The battle scripts of the maps](formats/scripts-and-archives.md#the-battle-scripts-of-the-maps).

**Battle set**: The 5 owners and 120 natives that `runtimeInit` registers for a battle script. See [Native functions: The battle registration](scripting/natives.md#the-battle-registration).

**Battle stage**: A `.pxy`/`.txy` pair of `yamamoto\map\`, selected by number through `dataMapNameTbl`. See [Maps and field data: Battle stages](formats/maps-and-field-data.md#battle-stages-yamamotomap).

**Battle start bonus**: Boost stocks or Stock/energy given by `battleStartBonusApply` when the cfEvent bits 1 or 2 are set. The party then also acts first. See [Battle: Start bonuses and the opening line](game/battle.md#start-bonuses-and-the-opening-line).

**Battle state**: `tgtSelectState` (`BattleState`, 0x64 bytes), the state of the turn machine and the battle menu, reached through `manWorkGet`. See [Battle: The turn machine](game/battle.md#the-turn-machine).

**Battle strings**: 54 strings behind 54 halfword offsets: drop lines and the official names of the statuses in bit order. See [Battle strings](formats/game-tables.md#battle-strings).

**Battle text layer**: `src/main/text_marker.c`: text window, text entries, choice menu drawing, ANALYZE panel, `textDrawString`. See [Fonts and text: The battle text layer](libraries/fonts-and-text.md#the-battle-text-layer). See also [UI windows: The battle text layer](libraries/ui-windows.md#the-battle-text-layer).

**Battle transition**: the breaking screen at the start of a battle, drawn by `gifStateDrawBreakShards` in callback slot 15. See [Screen effects: The battle transition](libraries/screen-effects.md#the-battle-transition).

**Battle unit**: One fighter, a `BattleUnit` object with a `StatBlock`. Eight exist at most: three party units and five enemies in `unitTbl`. See [Battle: Units, sides and keys](game/battle.md#units-sides-and-keys).

**Battle voice bank**: A 24-key bank in `sound\vsf\battle\` for one party member or enemy model, where the note number is the code of the situation. See [Sound files: Battle banks](formats/sound.md#battle-banks).

**battleAbsentUnitsApply**: Gives members who did not fight experience (full if they took part, else three quarters) and a quarter of their HP and EP. See [Party and growth: Paying out](game/party-and-growth.md#paying-out).

**BattleResult (battleResult)**: Running reward total of a battle: experience, Skill Points, Class Points and up to eight drops. See [Party and growth: Collecting rewards](game/party-and-growth.md#collecting-rewards).

**BattleResultRow**: 28-byte row per member: key, the three gains, levels gained and cap increases. See [Party and growth: The result screen](game/party-and-growth.md#the-result-screen).

**battleResultScene**: The result screen loop with the states count-up, tally, summary and item list. See [Party and growth: The result screen](game/party-and-growth.md#the-result-screen).

**battleSlotGet**: Returns the persistent parameter block of unit key 1 to 15. See [Party and growth: The parameter block](game/party-and-growth.md#the-parameter-block).

**BattleUnit**: The unit object of a battle, `0x1F0` bytes, one per fighter. It holds flags, the command list, the stats pointer, the pose and the actors. See [The battle engine: The unit object](game/battle-engine.md#the-unit-object).

**Behaviour block (`UwamonoState`)**: The `0x40C0`-byte block `record->body.motions` with the parameters, action, timers, routes and group data of a character. 32 exist (`uwamonoStates[]`). See [Field enemies: The behaviour block](game/field-enemies.md#the-behaviour-block).

**Behaviour kind (`state->mode`, "algorithm")**: Number set by `enepc.Algorithm` that selects what a character does after each pause: 0 nothing, 1 wander, 5 guard, 6 stand and turn, 8 patrol, 9 patrol back and forth, 12 flee, 13 spline. See [Field enemies: Behaviour kinds](game/field-enemies.md#behaviour-kinds).

**BG_MENUBG**: index 3 of the background layer states: the two hierarchies and the animation `menubg.xap` shared by the middle and front layers. See [Screen effects: Files and state](libraries/screen-effects.md#files-and-state).

**Bit container**: A word of an ARX code stream that is consumed bit by bit from bit 31. See [The code stream](formats/scripts-and-archives.md#the-code-stream).

**Blast point (`uwamonoPoints`)**: One of 16 records a trap explosion leaves for 300 frames with a radius of 3.0 or 15.0. Characters inside are stunned. See [Field enemies: Explosions and the stun](game/field-enemies.md#explosions-and-the-stun).

**Blend kind**: The four blend modes 0 to 3 (normal, additive, subtractive, difference) of `gsQueueSetBlendColor` and `gsMakeAlpha`. See [Rendering: The GS register queue](architecture/rendering.md#the-gs-register-queue).

**Blend length (`TiMRState.value`)**: Number of frames over which a new motion is mixed with the old pose. Set with `XenoActorSetBlendFrames`. See [Animation: How a blend is prepared](libraries/animation.md#how-a-blend-is-prepared).

**Blend matrices (`pBlendMat`)**: The third matrix buffer of an actor. It holds the pose of the old motion after a pointer swap. See [Animation: How a blend is prepared](libraries/animation.md#how-a-blend-is-prepared).

**Blink (fadeTimer)**: 90 frames after a battle in which the player is drawn translucent on and off. See [Field: The player update](game/field.md#the-player-update).

**Block table (`gameResBlockTable`)**: 128 `GameResBlock` entries (buffer, size, owner, tag, value) that describe the resource memory in address order. Entry 0 is the header. See [Files and resources: The block table](architecture/files-and-resources.md#the-block-table).

**Body collision tables**: The sphere (`TiCollSphere`), plane (`TiCollPlane`) and capsule (`TiCollCapsule`) tables of a hierarchy, shapes that move with a node and push dynamics nodes out. See [Skeletons and motion: Collision tables of the body](formats/skeletons-and-motion.md#collision-tables-of-the-body).

**Boost cursor**: The HUD object `menuCursor` that shows which party units can boost, or the unit that boosts. See [The battle engine: Boost cursor and boost gauges](game/battle-engine.md#boost-cursor-and-boost-gauges).

**Boost gauge**: `struct BoostGauge`, one per side: banked Boosts (max 3, up to 5) and 50 progress points per Boost. See [Battle: Boost](game/battle.md#boost).

**Boost slot**: One of the two priority slots of the action stock: slot 0 counter Boost, slot 1 Boost by command. See [Battle: Boosting by command](game/battle.md#boosting-by-command).

**`BootDisplay()`**: An empty function in OV02, the first call of `main()`. See [Boot and title: From main() to the title loop](game/title-and-boot.md#1-from-main-to-the-title-loop).

**botan.xtx**: `omiya\botan.xtx`, the texture with the five button pictures of the message windows. See [Fonts and text: Button tags in message windows](libraries/fonts-and-text.md#button-tags-in-message-windows). See also [UI windows: The window frame](libraries/ui-windows.md#the-window-frame). See also [Text and fonts: The sheet of the message windows](formats/text-and-fonts.md#the-sheet-of-the-message-windows).

**Bounding sphere test**: `_ModelCalcClipMat1`: a sphere (centre, radius in w) passes when all six outward plane distances are below the radius. See [Math: Sphere tests](libraries/math.md#sphere-tests).

**Break bank**: `break_<who>`, a 14-key bank with the field action sounds of the controlled character, loaded into slot 7. See [Sound files: Break banks](formats/sound.md#break-banks).

**Break bonus**: 1 Stock for the attacker that causes a Break, 50 percent chance, once per turn. See [Battle: The Break bonus](game/battle.md#the-break-bonus).

**Break sequence**: `StatBlock.liveRows`, the up to four zones that must be hit in order within a turn. See [Battle: Collecting the sequence](game/battle.md#collecting-the-sequence). See also [Unit records](formats/game-tables.md#unit-records).

**Break shard**: one of 24 `GifBreakShard` prisms that carry a piece of the picture in the battle transition. See [Screen effects: Shards](libraries/screen-effects.md#shards).

**Break, Air, Down**: The values 1, 2 (Down) and 3 (Air) of `StatBlock.state70`. Break gives 1.5 times damage, Air and Down double. See [Battle: State changes](game/battle.md#state-changes).

**Break-up program**: `xglBreakUpMicrocodeProgram`, mode 2, selected by status bit 0x8000. Draws single triangles and scatters those behind a plane. See [pxy: The Break-up program](libraries/pxy.md#the-break-up-program).

**bts_se**: The battle sound bank, `sound\vsf\battle\bts_se`, bank slot 2 during a battle. See [Audio: Battle sound](architecture/audio.md#17-battle-sound). See also [Sound files: Battle banks](formats/sound.md#battle-banks).

**Bucket**: One of 16 linked lists of the print queue, selected by bits 24 to 27 of the order key and flushed in ascending order. See [Fonts and text: Records and buckets](libraries/fonts-and-text.md#records-and-buckets).

**Buffer area**: A part of the sector arena (`xglCdBufferAlloc`) from which `xglCdBufferReserve` takes pieces and which `xglCdBufferRewind` frees at once. See [Files and resources: The sector arena of the menus](architecture/files-and-resources.md#the-sector-arena-of-the-menus).

**Built-in emitter image list**: The 22-row list of script `ce200_900` (kind 0x49) in `cfdef.esp`. See [Effect data files: the image list of the built-in emitters](formats/effects.md#the-image-list-of-the-built-in-emitters).

**Built-in particle emitter**: A coded particle effect of kind 0 to 24 with fixed spawn and update functions, used by field map objects. See [sdv: Built-in particle emitters](libraries/sdv.md#built-in-particle-emitters).

**Bump cursor**: The rising sound memory address (from `0x37800`, limit `0x100000`) from which enemy voice banks are placed in battle. See [Audio: Battle sound](architecture/audio.md#17-battle-sound).

**Button glyph tag**: A pad button name in message text (`<○>`, `<□>`, `<△>`, `<×>`, `<R3>`) that the wrapper replaces by spacing and a picture from `botan.xtx`. See [UI windows: Feeding text](libraries/ui-windows.md#feeding-text).

**Button map**: `buttonMap[8]`, remapping of the low eight button bits. See [xgl: Reading](libraries/xgl.md#reading).

**Button tag**: `<○>`, `<□>`, `<△>`, `<×>` or `<R3>` in dialogue text, replaced by a picture of `omiya\botan.xtx`. See [Fonts and text: Button tags in message windows](libraries/fonts-and-text.md#button-tags-in-message-windows). See also [Text and fonts: Button tags](formats/text-and-fonts.md#button-tags).

## C

**calcLvUp**: Spends an experience gain, possibly over several levels, up to level 99. See [Party and growth: Levels and experience](game/party-and-growth.md#levels-and-experience).

**calcPara2OrgSub**: Commits HP and EP of a working block to the persistent block. See [Party and growth: Writing back](game/party-and-growth.md#writing-back).

**calcResult**: Pays a finished battle: items to the boxes, shares to the survivors, levels, and experience to the reserve. See [Party and growth: Paying out](game/party-and-growth.md#paying-out).

**calcSkillEquChk**: Tests whether a unit has a skill active (learned for always-on skills, otherwise equipped or granted by an item). See [Party and growth: Totals for the menu](game/party-and-growth.md#totals-for-the-menu).

**calcSkillInit / calcSkillInitSub**: Apply skill records to a block: parameter bonuses, status protection, status resistance and five fixed skills. See [Party and growth: Totals for the menu](game/party-and-growth.md#totals-for-the-menu).

**Call site**: One opcode 0x15 instruction in a script file. All native statistics count call sites. See [Natives in the scripts](formats/scripts-and-archives.md#natives-in-the-scripts).

**Callback queue**: `xglCallbackQueueAdd` and `xglCallbackQueueRun`, a queue of up to four draw functions run later in the frame. See [Main menu: The menu frame](game/main-menu.md#the-menu-frame).

**Callback record**: Control code 23, queued by `xglFontPrintExtFunc`. Calls a function with the draw cursor during the flush, used for scissor rectangles. See [Fonts and text: Include and callback](libraries/fonts-and-text.md#include-and-callback).

**Callback slot**: one of the seventeen function pointers of the final packet pass. A layer switches on by storing its draw function there. See [Screen effects: Where the layers run in a frame](libraries/screen-effects.md#where-the-layers-run-in-a-frame). See also [The battle engine: The step stack](game/battle-engine.md#the-step-stack).

**camera**: Module of 50 natives for the scripted camera, registered on the field and in battle. See [Native functions: The camera module](scripting/natives.md#the-camera-module).

**Camera (shot)**: One of up to four alternative sets of camera tracks that an effect script offers, chosen by weighted chance. See [sdv: Selecting a camera](libraries/sdv.md#selecting-a-camera).

**Camera area**: A region of collision group `0x600`. The ground status of the area probe plus 1 is the record index. See [Field: Camera areas and probes](game/field.md#camera-areas-and-probes). See also [Maps and field data: Camera areas](formats/maps-and-field-data.md#camera-areas-group-0x0600).

**Camera entry**: Named script entry `<stem>_cam` (or `free_cam`) that stages the attacks of an enemy type. See [Entry names](formats/game-tables.md#entry-names).

**Camera fade (`alphaScale`)**: Fades an actor out when the camera is nearer than 1.6 times its radius. See [Actors: The camera fade](libraries/actors.md#the-camera-fade).

**Camera index (studio)**: One of four studios whose first active camera defines a view drawn by `nmlModelFlushSub`. See [Rendering: Model passes](architecture/rendering.md#model-passes).

**Camera owner record**: The effect record that currently drives the battle camera (`sefCameraOwnerRecord`, record flag `0x200`). See [sdv: Running the tracks](libraries/sdv.md#running-the-tracks).

**Camera quad**: effect mode 2, a ghost of the screen shifted along a world direction. See [Screen effects: Mode 2: camera quad](libraries/screen-effects.md#mode-2-camera-quad).

**Camera record**: One of 32 stored cameras in `cameraCfSlots` (208 bytes), one per camera area. See [Field: CameraRange and the camera records](game/field.md#camerarange-and-the-camera-records).

**Camera record (`TiCameraRecord`)**: An animated camera: six pose curves and a field of view curve. Used by the title screen. See [Animation: Animated cameras](libraries/animation.md#animated-cameras).

**Camera schedule**: `SdvCameraSchedule`, the selected camera and its four tracks (position, look-at, lens, shake). See [sdv: The camera schedule](libraries/sdv.md#the-camera-schedule).

**Camera shake**: Blending between random offsets every `shakeFrames` frames. See [Cutscenes: Camera shake](scripting/cutscenes.md#camera-shake). See also [Field: Direct mode and scripted cameras](game/field.md#direct-mode-and-scripted-cameras).

**Camera snapshot (`MEfCamParams`)**: Camera pointer, view matrix, rotation and camera rotation matrix that mode 0 copies from camera 0 of the studio before the callbacks run. See [Battle effects: The camera snapshot](game/battle-effects.md#the-camera-snapshot).

**Camera state**: `XglCamera.state`: 0 off, 1 idle, 2 manual, 3 chase, 4 game. See [Field: Studios and studio cameras](game/field.md#studios-and-studio-cameras).

**Camera surface**: Collision group `0x200` the camera position is kept on. See [Field: Keeping the camera out of the scenery](game/field.md#keeping-the-camera-out-of-the-scenery). See also [Maps and field data: Camera surfaces](formats/maps-and-field-data.md#camera-surfaces-group-0x0200).

**Camera table**: Five pairs of camera offset and weight in percent. See [Effect data files: the camera table and its tracks](formats/effects.md#the-camera-table-and-its-tracks).

**CameraRange**: The `0x70` byte block that describes one camera: anchor, locks, limits, position, look-at, angles. See [Field: CameraRange and the camera records](game/field.md#camerarange-and-the-camera-records).

**Canonical key**: The key under which effect data is stored, from `sefCanonicalEffectId`. Several keys share one. See [sef: Canonical keys and aliases](libraries/sef.md#canonical-keys-and-aliases).

**Caption file**: `movie/<language>/<name>.txt`, time-stamped lines for a movie. See [Cutscenes: The caption file](scripting/cutscenes.md#the-caption-file). See also [section 3](formats/movies-images-and-icons.md#3-movie-caption-files).

**Caption header string**: The entry at `00:00:00`, a font control string (`$ay390,$xx0,22,`) that sets the position and line spacing. See [section 3.2](formats/movies-images-and-icons.md#32-the-text-format).

**`@` caption marker**: A leading `@` in a subtitle or caption makes a line that is shown even when subtitles are off. Same section and [Movie captions](scripting/cutscenes.md#movie-captions).

**Caption stamp**: `mm:ss:ff`, converted to `(mm × 60 + ss) × 30 + ff`, compared with the frame stamp of the movie. See [section 3.2](formats/movies-images-and-icons.md#32-the-text-format).

**caption_all**: Option byte at `0x3A`. Zero shows only caption lines that start with a space, nonzero shows all lines. See [Save data: The header and the options](architecture/save-data.md#the-header-and-the-options).

**Card row**: The six page cards at the top of the menu (Items, Ether, Skills, Tech, Status, Party). See [Main menu: The card row and the command row](game/main-menu.md#the-card-row-and-the-command-row).

**carddata.bin**: Template of a save folder: the 964 bytes of `icon.sys` and one icon. See [section 8](formats/movies-images-and-icons.md#8-carddatabin-the-memory-card-icon-template).

**Catmull-Rom tangents**: Tangents taken as half the difference of the neighbouring points, `MMathCalcHermitePrm`. See [Math: Hermite curves](libraries/math.md#hermite-curves).

**CD arena**: Sector-aligned bump allocator for menu screens (`xglCdArenaOpen`, `xglCdSectorAlloc`, `xglCdArenaClose`). See [Memory: The CD arena and the CD buffer](architecture/memory.md#the-cd-arena-and-the-cd-buffer).

**CD buffer**: A rewindable bump allocator inside the CD arena (`xglCdBufferAlloc`, `xglCdBufferReserve`, `xglCdBufferRewind`). See [Memory: The buffer](architecture/memory.md#the-buffer).

**CD service (`xglCdControlThread`)**: The once-per-frame step of the file library, called from `xglSleep`. It is a plain function, not a thread. See [Files and resources: One step of the service](architecture/files-and-resources.md#one-step-of-the-service).

**CD service step**: `xglCdControlThread`, one step of the asynchronous read or stream service per frame. See [xgl: The service step](libraries/xgl.md#the-service-step).

**`CdFilePosition`**: The `0x30`-byte result of a path lookup: first sector, stored size, source, packed flag, descriptor, unpacked size. See [Files and resources: From a path to a position](architecture/files-and-resources.md#from-a-path-to-a-position).

**`CdFileSetHeader`**: The pointer header at the start of a file set block (`0x40` bytes reserved): model, textures, hierarchy, `.epf`, motion. See [Files and resources: Character and object sets](architecture/files-and-resources.md#character-and-object-sets).

**GameResBlock**: One entry of the block table: `buffer`, `size`, `owner`, `tag` (read callback), `value`. See [Memory: The resource block table](architecture/memory.md#the-resource-block-table).

**Cell**: One of 758 rectangles of `fontObjCellTable`: texture index, texel rectangle and colour table address. See [Fonts and text: Cells](libraries/fonts-and-text.md#cells).

**Cell grid**: `recordGridCells`, the 15 by 15 flags of the random cells record (type 4). See [Screen effects: Record types](libraries/screen-effects.md#record-types).

**Cell mask**: Eight bits that say from which of the eight cells around a unit it can be targeted. See [Maps and units: Targets](game/map-and-units.md#targets).

**cf event bits (`encounterFlags`)**: Battle modifiers read with `cfEventGet`: `0x1` boost bonus, `0x2` party bonus, `0x20` time limit, `0x200` skip victory, `0x800` player form not 1.0. See [Field enemies: The request](game/field-enemies.md#the-request).

**cfe**: The real-time cutscene (event) player. An SB plug-in of 29 natives plus the evaluation code that replays key tracks on actors, camera, lights, maps, effects, monitors, fog, screen effects and music. See [Cutscenes](scripting/cutscenes.md).

**cfe (module)**: Module of 29 natives, the script interface of the cutscene player. See [Native functions: The cfe module](scripting/natives.md#the-cfe-module).

**cfe plug-in**: The native table `cfePlugInTable` registered under the owner name `cfe` by `cfeRegisterPlugIn`. See [Cutscenes: The cfe commands](scripting/cutscenes.md#the-cfe-commands).

**`cfe.init` / `cfe.destroy`**: The natives that turn the field into a cutscene stage and back. See [Cutscenes: Starting a scene](scripting/cutscenes.md#starting-a-scene-cfeinit).

**`cfe.stFrm`**: The native that plays one frame of a cutscene and returns the next time. See [Cutscenes: One frame](scripting/cutscenes.md#one-frame-cfestfrm).

**CfeTrackNode**: The 0x30-byte key record: flags, frame, 8.8 value, two tangents, links. See [Cutscenes: The key node](scripting/cutscenes.md#the-key-node).

**CFEVE set**: One of 21 map sets `CFEVE01` to `CFEVE21` with a model and textures only, used as cutscene backdrops. See [Maps and field data: Naming](formats/maps-and-field-data.md#naming).

**cfEvent word**: `gameWork.encounterFlags`, the event flag word of an encounter (start bonuses, time limit, skip victory, no escape), read with `cfEventGet`. See [Battle: How a battle starts](game/battle.md#how-a-battle-starts).

**CFEventTool**: The developer cutscene tool, first entry of `testMenuRun`. Leftovers: text log writer, track editing helpers, grids. See [Cutscenes: The CFEventTool leftovers](scripting/cutscenes.md#the-cfeventtool-leftovers).

**CFS_SE**: The common field sound bank, bank slot 2 at `0x7D000`, loaded at the start of `Game`. See [Audio: Sound banks](architecture/audio.md#6-sound-banks). See also [Sound files: Bank names](formats/sound.md#bank-names).

**Chain area**: The lower half of a packet buffer, where DMA tags and VIFcodes are written upward from `start`. See [Rendering: Packets](architecture/rendering.md#packets).

**Channel A / channel B**: The two animation states of an actor: A is the body motion, B the face animation. See [Actors: Motion channels and modes](libraries/actors.md#motion-channels-and-modes).

**Channel flags**: Nine bits of a node block header that say which of translation, rotation and scale X, Y, Z have a curve. See [Skeletons and motion: Node blocks](formats/skeletons-and-motion.md#node-blocks).

**Channel Reset block**: The group of events at tick 0 of a song that sets bend, controllers 1, 5, 6, 7, 10, 11, 27, 91, bank select and program for one channel, labelled by a text event. See [Sound files: How a song is laid out](formats/sound.md#how-a-song-is-laid-out).

**Channel value (blend length)**: The number of frames over which a new motion is mixed with the old pose, set by `XenoActorSetBlendFrames`. See [Actors: Blending](libraries/actors.md#blending).

**Charge**: The E.S. form of the Stock command (`CMD_KIND_WAIT`). See [Battle: Stock and Charge](game/battle.md#stock-and-charge).

**Chase (`Enemy_Chase`)**: Action 5. The enemy steers to `chaseTarget`, the player or a corner found by `Check_MoveSquare`, and gives up by leash, area, ladder or lost sight. See [Field enemies: The chase](game/field-enemies.md#the-chase).

**`Check_Encount`**: Fills the battle request in `gameWork` on contact or for `enepc.Encount`. See [Field enemies: Contact and the battle request](game/field-enemies.md#contact-and-the-battle-request).

**`Check_InsideFan`**: Fan test with distance, angle, map units, map sweep and height limit. See [Field enemies: Sight](game/field-enemies.md#sight).

**`Check_InsideID`**: The geometric test of a locator shape (line crossing, circle, square, strip). See [Events: Locators](scripting/events.md#locators).

**`Check_Locater`**: The per-frame locator test for the player. See [Events: Locators](scripting/events.md#locators).

**Child actor (battle)**: One of up to three extra actors of a unit, for weapons and attached parts, bound to slot 27 or to a joint of the root actor. See [The battle engine: Binding the files](game/battle-engine.md#binding-the-files).

**Child actor / parent actor**: Actors linked through `children[8]` and `parent`. A child is updated, placed and drawn through its parent. See [Actors: Parents, children and attachments](libraries/actors.md#parents-children-and-attachments).

**Child effect**: Hit effect `pe310_001` to `pe310_011` started by a record from its `spawnBits` when a flagged slot starts. See [sef: Child effects](libraries/sef.md#child-effects).

**Child model / parent**: A model submitted after `nmlModelSetParent` joins the batches, light and shadow of its parent through the slot ring. See [pxy: Batches](libraries/pxy.md#batches).

**Child model pair**: (model number, attach value) of a weapon or prop of a party unit, three per unit. See [Child model pairs](formats/game-tables.md#child-model-pairs).

**Choice menu**: One of the two battle list menus of `choiceMenuSlots` (items, optional values and flags, columns, a `ListCursor`, a scroll track). See [UI windows: Choice menus](libraries/ui-windows.md#choice-menus).

**Choice prompt**: The yes/no box of the menu (`choiceInit`, `choiceUpdate`, `choiceGetResult`). See [Main menu: Prompt and notice](game/main-menu.md#prompt-and-notice).

**Chunk (effect file)**: A 64-byte header plus data, a multiple of 0x40 bytes long. Kinds: 0 end, 1 file header, 3 CLUT, 4/7 image, 5 script, 6 `.pxy`, 9 `.txy`, 10 embedded `.esd`. See [Effect data files: the chunk container](formats/effects.md#the-chunk-container).

**Chunk id**: The low byte of `typeFlags` in a chunk header, unique inside a file. Image references name chunks by it. See [Effect data files: the chunk container](formats/effects.md#the-chunk-container).

**Circle shadow**: Round blob shadow drawn as a quad with a 64 by 64 palette texture. See [pxy: Circle shadows](libraries/pxy.md#circle-shadows). See also [Actors: Circle shadow](libraries/actors.md#circle-shadow).

**Citizen**: One of 50 people with a record in `Citizen.txt` and a portrait. 36 of them are campaign clients. See [G.S. Campaign: The citizen profile page](game/gs-campaign.md#the-citizen-profile-page).

**Citizen flag**: One byte per citizen in `citizenFlags[50]`. Non-zero makes the profile visible. See [G.S. Campaign: Citizen flags](game/gs-campaign.md#citizen-flags).

**Citizen flags**: `citizenFlags`, 50 bytes, natives `g2path.setCitizenFlag` / `getCitizenFlag`. See [Events: Quest and citizen flags](scripting/events.md#quest-and-citizen-flags).

**Citizen.txt**: Tagged text file with 50 citizen records (name, sex, age, hobby, profile). See [G.S. Campaign: iwase\Citizen.txt](game/gs-campaign.md#iwasecitizentxt).

**`ck` / `rk` names**: The two scales of kowaremono rows: `ck` objects are broken on foot, `rk` objects (five times as large) with an E.S. See the same section.

**Class points (C.Pt)**: The points that open a class (`PartyMemberExpPoints.classPoints`). See [Main menu: Skills](game/main-menu.md#skills). See also [Party and growth: Opening a class](game/party-and-growth.md#opening-a-class).

**Clear skip**: The flag `xglRenderClearSkip` that suppresses the start-of-frame clear (battle scene, some field states). See [Rendering: GS environments](architecture/rendering.md#gs-environments). See also [xgl: Clearing the screen](libraries/xgl.md#clearing-the-screen).

**ClearEnv**: The six-quadword clear sprite sent over GIF DMA at the start of each frame, in full or depth-only form. See [Rendering: GS environments](architecture/rendering.md#gs-environments).

**Clip flags**: Six bits from `vclipw`, one per side of the view volume, returned `& 0x3f` by the projection functions. See [Math: Projecting one point](libraries/math.md#projecting-one-point).

**Clip plane fade**: Alpha multiplied by the clamped distance to the plane at 0x352, on with flag bit 0x200. See [pxy: Lighting](libraries/pxy.md#lighting).

**Clip planes (`PxyClipPlanes`)**: Up to two planes per model, in three forms (with angle, pair, masked). See [pxy: Clip planes and reflection](libraries/pxy.md#clip-planes-and-reflection).

**Clip sweep**: A clip plane that travels through a model in one of eight directions so it appears or vanishes slice by slice. See [Actors: The clip sweep](libraries/actors.md#the-clip-sweep).

**Cloth id**: Resource id of a member's model, chosen by `ptGetClothId` from the story progress. See [Party and growth: Cloth ids and models](game/party-and-growth.md#cloth-ids-and-models).

**CMSAR0 (vi27)**: The VU0 control register that holds the start address for `vcallmsr`, counted in 8-byte instruction pairs. See [Math: Micro mode](libraries/math.md#micro-mode-and-the-resident-microprogram).

**Co-pilot**: The freely chosen second character of an E.S. Decides which special attacks the E.S. can use. See [Party and growth: Pilots and co-pilots](game/party-and-growth.md#pilots-and-co-pilots).

**Code unit**: One unsigned 16-bit word of script code. Program counters, jump targets and thread entries are unit indices. See [The SB virtual machine: Sections](scripting/sb-vm.md#sections). See also [Code](formats/scripts-and-archives.md#code).

**Collision and wind set (DEV)**: A `.dev` file, magic `DEV `, version 0.5 (`TiGlobalCollSet`): world space spheres, planes and cylinders for the dynamics nodes plus eight wind blocks. See [Skeletons and motion: Collision and wind sets (DEV)](formats/skeletons-and-motion.md#collision-and-wind-sets-dev).

**Collision entry**: A 16-byte `TiCollEntry` behind a dynamics node that indexes a sphere, plane or capsule of the same hierarchy. See [Skeletons and motion: Dynamics nodes](formats/skeletons-and-motion.md#dynamics-nodes).

**Collision group**: A set of meshes of one purpose inside the collision block, named by a type word (`0x100` ground, `0x300` placement table, `0x700` footsteps, `0x800` enemy ground, `0xA00` light areas, `0xB00` keep-off). See [Maps and units: Blocks, groups and entries](game/map-and-units.md#blocks-groups-and-entries).

**Collision shape (searchMode)**: 0 none, 1 box, 2 circle. See [Maps and units: Collision with units](game/map-and-units.md#collision-with-units).

**Collision sphere, plane, capsule (`TiCollSphere`, `TiCollPlane`, `TiCollCapsule`)**: Shapes of the model's own body that dynamics nodes are pushed out of. See [Animation: Collision shapes of the body](libraries/animation.md#collision-shapes-of-the-body).

**Color pass**: The second loop over a batch, which lights every vertex and fills the `RGBAQ` slots. See [pxy: Lighting](libraries/pxy.md#lighting).

**Combined ether**: An ether that the acting unit casts together with the unit in Boost slot 1 (`cmdCombinedEtherRun`). See [Battle: Combined ether](game/battle.md#combined-ether). See also [Ethers](formats/game-tables.md#ethers).

**Combo**: `StatBlock.comboHits`, the chain of hits with a shared attribute. Each hit after the first adds 10 percent. See [Battle: Combos](game/battle.md#combos).

**Combo panel**: The HUD panel with a title and the hit count of a unit's combo. See [The battle engine: Hit panels](game/battle-engine.md#hit-panels).

**Command list**: `cmdList`, the ring of 32 queued unit commands (`CmdListBody`) filled by `cmdListSet`. See [Battle: From a choice to a command](game/battle.md#from-a-choice-to-a-command).

**Command panel**: The two-column battle menu opened with Cross (selections 11 to 19: Stock/Charge, Ether, Item, Double, Move, Change, E.S. special, Escape, special attack). See [Battle: The command panel](game/battle.md#the-command-panel).

**Command row**: The second row of one to four commands (U.M.N., G.S. Campaign, segment list, Options), built in `menuCardAvailabilityBits`. See [Main menu: The command row](game/main-menu.md#the-command-row).

**Common map object resources**: The 54 files of `matumoto\` (4 data files and 25 model and texture pairs) loaded by `mapObjectResourcesLoad`. See [Field: Common map object resources](game/field.md#common-map-object-resources).

**Compact mode**: Drawing kana and alphanumerics from the 8x8 strips, scaled by `compact_width` and `compact_height`. Used by code 24 and by ruby annotations. See [Fonts and text: Small glyphs](libraries/fonts-and-text.md#small-glyphs).

**Completion callback (`CdCompletionCallback`)**: The function `(event, value, tag)` a read reports to: lookup (0), size (1), progress (2 and 3), done (4), read error (-2). See [Files and resources: Reading a file](architecture/files-and-resources.md#reading-a-file).

**configScreenRun**: The screen between New Game and the first map: asks whether to search the cards for old data, shows the result, then offers captions, sound and Dolby Digital options. See [Save data: The configuration screen](architecture/save-data.md#the-configuration-screen).

**consumedSize**: Output of `xglJpegDecode`, the offset where the marker loop stopped. The map loader uses it to find the alpha plane. See [section 5.2](formats/movies-images-and-icons.md#52-what-the-game-decoder-accepts).

**Contact class (`reactionKind`)**: Value 0 to 4 from the sectors in which player and enemy met (front, side, back). See [Field enemies: Who caught whom](game/field-enemies.md#who-caught-whom).

**Contact table**: The list of joint chains and widths the drop shadow uses, chosen by the actor's `kind`. See [Actors: Drop shadow](libraries/actors.md#drop-shadow).

**Control bits**: The single-bit switches of `gameWork.control` (`CONTROL_PAUSED`, `CONTROL_CFE_SCENE`, `CONTROL_SCRIPTED` and others). See [Field: The control bits](game/field.md#the-control-bits).

**Control code**: A byte from 1 to 31 inside a queued string that changes the draw cursor or the font state, with operand bytes counted by `xglFontGetSPcodeSize`. See [Fonts and text: Control codes](libraries/fonts-and-text.md#control-codes).

**Control points (`grControlPoints`)**: Three float pairs blended with weights (1-f)^3, rest, f^3 to give the half size of the clip rectangle of an opening or closing frame. See [UI windows: The opening curve](libraries/ui-windows.md#the-opening-curve).

**Control slot**: One of the eleven `ControlSlot` entries of `menuSlots`, a page or layer with a step function, a draw function, a state and a transition counter. See [Main menu: Control slots](game/main-menu.md#control-slots).

**`CONTROL_CFE_NO_FADE`**: Bit 0x100000 of `gameWork.control`. Suppresses the fade-out of scripted map changes. Same section.

**`CONTROL_CFE_SCENE`**: Bit 0x200 of `gameWork.control`. While set, `Game()` skips field update and drawing. See [Cutscenes: How a cutscene fits into the game](scripting/cutscenes.md#how-a-cutscene-fits-into-the-game). See also [Field: The frame loop](game/field.md#the-frame-loop).

**CONTROL_PAUSED**: Bit `0x1`, the pause is on. See [Field: Pause](game/field.md#pause).

**CONTROL_REPLAYED**: Bit `0x10000`, the last stick input keeps being applied during a place change. See [Field: Moving](game/field.md#moving).

**`CONTROL_SCRIPTED`**: Control bit `0x8000`. Set while a message is open or a place change is pending. The player cannot walk. See [Events: Showing the message](scripting/events.md#showing-the-message). See also [Field: The control bits](game/field.md#the-control-bits).

**CONTROL_MAP_ATTACK**: Bit `0x20000`, the player is in an attack on a map object. See [Field: Attacking map objects](game/field.md#attacking-map-objects).

**CONTROL_SOFT_RESET**: Bit `0x10000000`, set by L1+L2+R1+R2+SELECT+START, ends the session. See [Field: The frame loop](game/field.md#the-frame-loop).

**Converted caption list**: The in-place compact form made by `caption_convert`. See [Cutscenes: The converted form](scripting/cutscenes.md#the-converted-form).

**`copyframe()` / `GameCopyFrame()`**: Copy the 512 by 448 picture on screen into the page `sRender.vram.work_buffer_base` so that the next screen can cross-fade from it. See [Boot and title: Drawing](game/title-and-boot.md#drawing) and [Drawing a decoded picture](game/title-and-boot.md#13-drawing-a-decoded-picture).

**Cosine ease / easeAngleStep**: An angle from 0 to 180 degrees stepped by 45 (distance under 31) or 23 per frame. See [Math: Easing steps](libraries/math.md#easing-steps). See also [UI windows: Easing helpers](libraries/ui-windows.md#easing-helpers).

**Countdown panel**: The clock at x 0x151, y 8 that shows `gameWork.panelValue` as minutes, seconds and hundredths, started by the `TimeAttack` native. See [UI windows: The countdown panel](libraries/ui-windows.md#the-countdown-panel).

**Counter Boost**: A free Boost into slot 0 that a unit may get when it is hit (`counterProc`). See [Battle: Counter Boost](game/battle.md#counter-boost).

**Counter gauge**: `StatBlock.rollPercent` of units with trait 0x800. In the stock stance it is rolled to turn an incoming technique around. See [Battle: The counter gauge](game/battle.md#the-counter-gauge).

**credit.bin**: Archive of the six logo pictures shown at start-up. See [section 6.3](formats/movies-images-and-icons.md#63-creditbin).

**CRI ADX**: CRI's audio middleware: `ADXT` (stream player), `ADXF` (file system), `ADXM` (manager), `ADXPS2` (platform layer). See [Runtime libraries: CRI ADX and Sofdec](architecture/runtime-libraries.md#cri-adx-and-sofdec). See also [Audio: ADX streams](architecture/audio.md#11-adx-streams).

**CRI file layer**: the ADXF device handle through which Episode II reads the disc. See [From Episode I to Episode II: 4. Files and archives](architecture/from-episode-i.md#4-files-and-archives).

**CRI_ADXI**: IOP module of the ADX player, loaded with `spucore=1`. See [Runtime libraries: IOP modules on the disc](architecture/runtime-libraries.md#iop-modules-on-the-disc).

**CRITAGS record**: Text record of the CRI video encoder in pack 3. See [section 2.3](formats/movies-images-and-icons.md#23-the-sofdec-header-packs).

**crt0**: The start-up code at the entry point `0x00120008` that calls `main`. See [Runtime libraries: Start-up](architecture/runtime-libraries.md#start-up).

**Culling volume**: A rectangle in the world that hides model parts behind it (`CullingVolume`, at most 10 per map, 13 maps have them). See [Maps and units: Culling volumes](game/map-and-units.md#culling-volumes).

**_Cur functions**: The model library's projection and clip functions on the `vf27`-`vf30` matrix. See [Math: Projection and clipping](libraries/math.md#projection-and-clipping).

**Current link**: The list that receives UP and DOWN and shows the cursor bar (`menuCurrentLink`, bit 0 of `mode_flags`). See [Main menu: ListCursor](game/main-menu.md#listcursor).

**Current matrix**: A matrix kept in VU0 registers (`xglCur*`). Owned by the math page. See [xgl: Math helpers](libraries/xgl.md#math-helpers). See also [Math: Three current matrices](libraries/math.md#three-current-matrices).

**Current packet / send packet**: The half of the packet buffer being filled and the half being transferred. They alternate each frame. See [xgl: Current and send](libraries/xgl.md#current-and-send).

**Current studio / active studio**: The studio the accessors work on (`xglStudioChange`) and the studio whose cameras react to the pad (`xglStudioSetActiveIndex`). See [xgl: Current and active studio](libraries/xgl.md#current-and-active-studio).

**Cursor**: Per-reader position on a track: key index, running time and last sample. See [sef: Cursors](libraries/sef.md#cursors).

**Cursor memory**: The two settings `cursor_memory` and `cursor_memory_alt` that keep list positions between menu sessions and skip the first step of pages. See [Main menu: Cursor memory](game/main-menu.md#cursor-memory).

**Curve**: A run of keys that ends at the key with the END flag. It has no key count. See [Skeletons and motion: Curves](formats/skeletons-and-motion.md#curves).

**Curve stream**: A run of node blocks without an index, one block per node in hierarchy order. An entry has a joint, second joint, third joint, dynamics and face stream. See [Skeletons and motion: Node blocks](formats/skeletons-and-motion.md#node-blocks).

**Cut list (`titleActorListNN`)**: Byte list of one cut: first motion index, records needed, actor keys, pairs of actor key and placement. See [Boot and title: The demo scene](game/title-and-boot.md#the-demo-scene).

**Cutscene camera (`cfeCamera`)**: Studio camera 0 with ten tracks and the shake state. See [Cutscenes: The camera](scripting/cutscenes.md#the-camera).

**Cutscene motion pack**: A motion pack in `motion\cfevent\`, loaded by name with `cfe.ldMt`. Scene packs are named `NNNNNN_<actor>.xap`, shared ones `<actor>_all.xap`. See [Skeletons and motion: Cutscene motion packs](formats/skeletons-and-motion.md#cutscene-motion-packs).

**Cutscene node pool**: 6,144 `CfeTrackNode` records of 48 bytes in one anonymous resource block of `0x48000` bytes. See [Memory: The cutscene node pool](architecture/memory.md#the-cutscene-node-pool).

**Cutscene pause and skip**: START pauses a cutscene or movie, TRIANGLE on the pause screen skips it. See [Cutscenes: Pause and skip](scripting/cutscenes.md#pause-and-skip).

**Cutscene script**: A script with a `scene` thread that drives the `cfe` player (93 files). See [The cutscene script](formats/scripts-and-archives.md#the-cutscene-script).

**Cutscene track**: A long stereo `.adx` in `sound\adx\cfevent\S-cc-ss-pp\` that carries the mixed sound of one cutscene. See [Sound files: The streams on the disc](formats/sound.md#the-streams-on-the-disc).

## D

**Damage number**: A HUD object made by `objDmgNumCreate` that prints a number at the screen position of a unit for 25 frames. See [The battle engine: Target and command panels](game/battle-engine.md#target-and-command-panels).

**Data base**: The table size of an offset-table archive rounded up to 64, where item 0 starts. See [section 6.1](formats/movies-images-and-icons.md#61-the-common-layout).

**Data box**: One of four inventory arrays of 255 counts, limit 99: 1 usable items, 2 E.S. accessories, 3 special items, 4 keys. See [Party and growth: The boxes](game/party-and-growth.md#the-boxes). See also [Battle: The inventory in battle](game/battle.md#the-inventory-in-battle).

**Data object**: One of two `0x1E0`-byte slots of `dataObjTable`. The battle cursor made by `objEnCurCreate` lives in it. See [The battle engine: The object model](game/battle-engine.md#the-object-model).

**Data slot**: A `DataSlot`, the file table of one unit: four models, four motion files and a voice bank. There are 11. See [The battle engine: Data slots](game/battle-engine.md#data-slots).

**dataCdSync**: The count of asynchronous disc reads the battle loader has started and not yet seen finished. See [The battle engine: Reading files](game/battle-engine.md#reading-files).

**DataLookup**: A block of up to 12 items of `0x18` bytes with a read cursor. It is the command list of a unit and of the battle cursor. See [The battle engine: Small containers](game/battle-engine.md#small-containers-in-obj_slotc).

**dataPlUnitInit**: Builds the fifteen party parameter blocks and point records from `bat.dat`. See [Party and growth: Unit initialisation](game/party-and-growth.md#unit-initialisation).

**Debt**: The amount `msgBalance`, 10,000,000 at the start of a new game, paid off by handing over items. See [G.S. Campaign: The debt](game/gs-campaign.md#the-debt).

**Debug file selector (`xglCdFileSelect`)**: An unused text-mode file browser that reads a `.filelist` file from the host. See [Files and resources: The debug file selector](architecture/files-and-resources.md#the-debug-file-selector).

**Debug menu**: The hierarchical text menu of `xgl_menu.c` (16 slots, 8 levels). See [xgl: The debug menu](libraries/xgl.md#the-debug-menu).

**Decoder**: The key of a segment address. Entry *n* of data box category 3 is the decoder of address *n*. See [G.S. Campaign: Flags that the segment screen borrows](game/gs-campaign.md#flags-that-the-segment-screen-borrows).

**decrypt**: Flips bit 7 of each byte of the stored partition name and patches one digit from `SYSTEM.CNF`. Not related to the save image. See [Save data: The game partition](architecture/save-data.md#the-game-partition).

**Default light set**: The standard lighting from `xglLightSetDefault`, cached after the first call. See [xgl: The light set](libraries/xgl.md#the-light-set).

**Default Program**: Program 0 of a bank. In every effect bank it is the identity map (note n plays key n), which is why a sound effect is "note N of bank B". See [Sound files: Programs](formats/sound.md#programs).

**Default width**: Font state byte (default 4) used as the right bound of a proportional glyph without a table entry, in practice the width of a space. See [Fonts and text: Proportional widths](libraries/fonts-and-text.md#proportional-widths).

**Defocus layer**: one of sixteen `DefocusLayer` slots in `GameDefocusParam`, each with a type and sixteen parameters, drawn in order by `DefocusMain`. See [Screen effects: Layers](libraries/screen-effects.md#layers).

**Defocus type**: the effect of a defocus layer, 1 to 11: page copy, mask, mosaic, box, lens, streaks, feedback, frozen frame, wave. See [Screen effects: The types](libraries/screen-effects.md#the-types).

**Degree tables (xglCosDeg, xglSinDeg)**: Lazy 37-entry tables with 10-degree resolution for 2D drawing. See [Math: Sine and cosine in degrees](libraries/math.md#sine-and-cosine-in-degrees).

**Depth mask**: the alpha 0 sprite of defocus type 2 (and record preset 18) that removes the alpha of pixels by a depth test so later layers skip them. See [Screen effects: The types](libraries/screen-effects.md#the-types).

**Descrambling**: The in-place decoding of a `.pxy` file whose flag bit 31 is set, done by `pxyDescrambleHeader`. See [pxy: The .pxy file](libraries/pxy.md#the-pxy-file).

**Detail page**: Result report of a finished campaign with the long `UkunSCD` or `UkunFLD` text and the guide model. See [G.S. Campaign: The detail page](game/gs-campaign.md#the-detail-page).

**`device_check_sync()`**: Frame helper of the card check: `xglMcMain()`, `xglSleep()`, font mode string. Returns both port states in one word. See [Boot and title: The memory card check](game/title-and-boot.md#4-the-memory-card-check).

**Direct entry**: A block of ready packet data recorded with `nmlModelDirectSend` and played at a fixed point of the model passes. Up to 3048 per frame. See [Rendering: Direct entries](architecture/rendering.md#direct-entries).

**Direct mode**: Camera flag `0x200` (`cfBattleCam`), only `cameraApplyView` runs. Shakes work only here. See [Field: Direct mode and scripted cameras](game/field.md#direct-mode-and-scripted-cameras).

**Direct queue**: The list of A+D pairs that `nmlPacketGsInit` starts in the attribute area and `packet_gs_entry64` fills (`xglDirectQueueBuffer`, `xglDirectQueueCount`). See [Rendering: Writing into a packet](architecture/rendering.md#writing-into-a-packet).

**Direct send**: Packet data of other systems registered with `nmlModelDirectSend*` and played at fixed points of the flush, by group. See [pxy: Direct sends](libraries/pxy.md#direct-sends).

**Directional wind / point wind**: The two kinds of wind source, four of each, global to all actors. See [Animation: Wind](libraries/animation.md#wind).

**Directory record**: the flags byte format shared by both archive directories. See [From Episode I to Episode II: 4. Files and archives](architecture/from-episode-i.md#4-files-and-archives).

**Directory record / file record**: The two record kinds of a directory file, told apart by bit 7 of the flags byte. A file record adds a 3-byte sector, a 4-byte stored size and, when packed, a 3-byte decoded size. See [The directory file](formats/scripts-and-archives.md#the-directory-file).

**Disc change**: `GameDiskChange`, the prompt that waits for disc 1 or 2, started by the `disk` command or by the synopsis screen. See [Events: The disc change](scripting/events.md#the-disc-change).

**Disc change screen (`GameDiskChange`)**: Shows `yajima\diskchg.jpg` and waits until the wanted disc is identified on two frames in a row, then reloads the directories. See [Files and resources: Disc identification and disc change](architecture/files-and-resources.md#disc-identification-and-disc-change).

**Disc flags**: The result of `xglCdDiskCheck`: bit 0 = disc 1 (`xenosaga.10` present), bit 1 = disc 2 (`xenosaga.20`), `0x7F` on a dual-layer disc, 0 or negative when no usable disc. See [Files and resources: Disc identification and disc change](architecture/files-and-resources.md#disc-identification-and-disc-change).

**Disc number**: The result of `xglCdDiskCheck()`: 1 when the disc holds `xenosaga.10`, 2 when it holds `xenosaga.20`. See [Boot and title: The two overlays and the two discs](game/title-and-boot.md#2-the-two-overlays-and-the-two-discs).

**Discovery (`Check_Discovery`)**: Common end of sight and hearing. Starts action 4 and then the chase unless a guard condition forbids it. See [Field enemies: Discovery](game/field-enemies.md#discovery).

**`;disk` command**: Scene resource command that calls `GameDiskChange(1 or 2)` when the scene's resources load. See [Boot and title: The disc change screen](game/title-and-boot.md#10-the-disc-change-screen).

**Dispatch state**: The byte `LW.dispatch_state`: idle (0), synchronous read (1), asynchronous read (2), stream (3). See [Files and resources: The request queue and the per-frame service](architecture/files-and-resources.md#the-request-queue-and-the-per-frame-service).

**DispEnv**: The display environment (`GsDispEnv`), sent every frame with the shown page and the scanline mode. See [Rendering: GS environments](architecture/rendering.md#gs-environments).

**Display offset**: `SaveData.displayOffsetX` and `displayOffsetY`. Display dx = base + 5 * X, dy = base + Y (`tyaSetDisplayOffset`). See [Boot and title: The display position screen](game/title-and-boot.md#14-the-display-position-screen).

**Display page**: `sRender.display_buffer_base`, the page the frame is drawn into and the target of every effect. See [Screen effects: Pages and coordinates](libraries/screen-effects.md#pages-and-coordinates).

**Display position screen (`tyaDisplaySetting`)**: Lets the player shift the picture by up to 20 steps in each direction with the test picture `mnt_set.jpg`. See [Boot and title: The display position screen](game/title-and-boot.md#14-the-display-position-screen).

**display_buffer_base, draw_buffer_base**: The two words that `xglRenderSwapBase` exchanges. The packet being built draws into `display_buffer_base`. See [Rendering: Frame buffers and the swap](architecture/rendering.md#frame-buffers-and-the-swap).

**Distance fade**: Map program, material flag 0x4000: alpha falls with the distance to the camera between a start and a range. See [pxy: The Map program](libraries/pxy.md#the-map-program).

**Door**: A pack entry named `door...`, set up with `uwamono.setDoorAll`, sliding or turning between a closed and an open pose. See [Maps and units: Doors](game/map-and-units.md#doors).

**doorimg.dat**: One file with an offset table and the 18 door pictures. See [G.S. Campaign: The door pictures](game/gs-campaign.md#the-door-pictures). See also [doorimg.dat](formats/game-tables.md#doorimgdat).

**Double attack mask (menuPartnerMask)**: The bit mask of unlocked double attacks, read with `slotPairTest`. See [Main menu: Set-up in menuSceneInit](game/main-menu.md#set-up-in-menusceneinit).

**Double buffer**: The `BASE`/`OFFSET` pair at the end of each program packet. VIF1 swaps the input buffer at every `MSCAL` and `MSCNT`. Model programs 0 and 0x114, water 0 and 0x1aa, effect 0x28 and 0x212. See [pxy: The double buffer](libraries/pxy.md#the-double-buffer).

**Double DQT segment**: A JPEG `DQT` segment with both tables, which `DefineQuantizeTable` reads as one table of 129 values, shifting the chrominance table by one. See [section 5.3](formats/movies-images-and-icons.md#53-two-quantization-tables-in-one-segment).

**Double Tech (DoubleTechRecord, pairBits)**: One of 16 two-character attacks. Known pairs are bits in both units' point records (`slotPairSet`). See [Party and growth: Double Techs](game/party-and-growth.md#double-techs).

**Double Tech record (`DoubleTechRecord`)**: Technique, Stock cost and the two partner keys of one of the 16 Double Techs. See [Double Techs](formats/game-tables.md#double-techs).

**Double technique**: A technique of two fixed characters (`DoubleTechRecord`), available when the partner is boosted and both have the required Stock. Once per battle. See [Battle: Double technique](game/battle.md#double-technique).

**Draw callback queue**: `xglCallbackQueue` (4 entries), filled with `xglCallbackQueueAdd` and run by `xglCallbackQueueRun`. Used by 2D screens. See [xgl: Final packet callbacks](libraries/xgl.md#final-packet-callbacks).

**Draw counters (`sefEntryCounts`)**: 58 entries with the packet space and model count of each image mapper for the current frame. See [sef: The draw counters](libraries/sef.md#the-draw-counters).

**Draw cursor (FontDrawCursor)**: The pen state of a flush, built at the scratchpad base: packet, position, depth, colour, effect, blend, scale and style flags. See [Fonts and text: The font state](libraries/fonts-and-text.md#the-font-state).

**Draw entry**: A pair of part and packet pointers in `pxyDrawEntryTable`. See [pxy: Draw entries](libraries/pxy.md#draw-entries).

**Draw environment / display environment / clear environment**: The prepared GS packets `DrawEnv`, `DispEnv` and `ClearEnv`. See [xgl: Environment blocks](libraries/xgl.md#environment-blocks).

**Draw flags (`drawFlags`)**: The per-actor draw switches (hidden, skip once, light, alpha, fade and others). See [Actors: Draw flags](libraries/actors.md#draw-flags).

**Draw list (`DrawList`)**: The run buffer of sprite and polygon records of the sv layer. See [pxy: The draw list](libraries/pxy.md#the-draw-list).

**Draw list (svDrawList)**: The row of 16-byte slots in the attribute area in which the effect system collects quads before one unpack and one program call. See [Rendering: The effect draw list](architecture/rendering.md#the-effect-draw-list).

**Draw mode**: Argument of `pxyDrawBatches`: 0 main, 1 water reflection, 2 glow source, 3 separate, 4 source pass. See [pxy: One camera](libraries/pxy.md#one-camera).

**Draw mode word**: Word 7 of the second angle header: 0 billboard, 1/2/4 oriented quads, 8 fixed quad. See [Effect data files: the point table](formats/effects.md#the-point-table).

**Draw page**: `sRender.draw_buffer_base`, the other page of the double buffer. As a texture it gives the previous frame. See [Screen effects: Pages and coordinates](libraries/screen-effects.md#pages-and-coordinates).

**Draw pass**: 0 for the opaque round, 1 for the translucent round (`pxyDrawPass`). See [pxy: The order inside a pass](libraries/pxy.md#the-order-inside-a-pass).

**Draw record (effect)**: A ready GS packet of 14, 17, 28 or 34 quadwords in which the effect program fills in the four corners of a sprite. See [pxy: The effect program](libraries/pxy.md#the-effect-program).

**`_draw3D`**: Mode of an effect pass: 0 sprites and polygons, 1 kind-8 sprites, 2 models. See [pxy: The three effect passes](libraries/pxy.md#the-three-effect-passes).

**`DrawBack(alpha)`**: Draws the frame saved by `GameCopyFrame()` over the picture, for cross-fades. See [Boot and title: Drawing a decoded picture](game/title-and-boot.md#13-drawing-a-decoded-picture).

**DrawEnv**: The fifteen-register draw environment sent after each swap. See [Rendering: GS environments](architecture/rendering.md#gs-environments).

**`DrawImage()`**: Sends a raw 512 by 448 picture (`0xE0000` bytes) to the display buffer as two halves of `0x7000` quadwords. See [Boot and title: Drawing a decoded picture](game/title-and-boot.md#13-drawing-a-decoded-picture).

**Drop circle**: A round shadow drawn as a volume (ring above ground, copy below). See [Actors: Drop circle](libraries/actors.md#drop-circle).

**Drop shadow**: A body-shaped shadow made of one volume per limb segment from a contact table. See [Actors: Drop shadow](libraries/actors.md#drop-shadow).

**dummy.bin**: Filler file that brings icons plus filler to `0x42400` bytes in every save folder. See [Save data: Creating a save folder](architecture/save-data.md#creating-a-save-folder).

**Dynamics bodies**: The `0x40`-byte records in `pDynamics` for loose pieces (hair, cloth) that the actor pushes when it moves or turns. See [Actors: Pushing the dynamics bodies](libraries/actors.md#pushing-the-dynamics-bodies).

**Dynamics node**: A `TiHrcDynamics` record, a simulated bone of hair or cloth, 0xB0 bytes plus one 16-byte collision entry per shape it is tested against. See [Skeletons and motion: Dynamics nodes](formats/skeletons-and-motion.md#dynamics-nodes).

**Dynamics node (`TiHrcDynamics`)**: A hair or cloth node simulated as a point mass at a fixed distance from its parent. See [Animation: Dynamics](libraries/animation.md#dynamics-hair-and-cloth).

**Dynamics weight**: A float in the block header of a dynamics node (entry versions 1.3 and 1.4) that says how much of the node follows its curves instead of the simulation. See [Skeletons and motion: Node blocks](formats/skeletons-and-motion.md#node-blocks).

## E

**E bit**: The "end program" flag of a pair. Every entry point starts with it, so `MSCAL` only sets where the next `MSCNT` resumes. See [pxy: The model programs](libraries/pxy.md#the-model-programs).

**E.S. energy**: `StatBlock.current` with cap `currentMax` (200 or 300), the resource of E.S. specials. Charge adds 100. See [Battle: E.S. special](game/battle.md#es-special).

**E.S. mode**: The menu form for the split party in the three E.S. (`ptState.splitMode == 1`). The party grid has 13 panels, Tech shows E.S. attacks and Status equips accessories. See [Main menu: Foot mode and E.S. mode](game/main-menu.md#foot-mode-and-es-mode).

**E.S. special record (`EsSpecialRecord`)**: Technique, energy cost (100, 200, 300) and sub pilot mask, seven per E.S. See [E.S. specials](formats/game-tables.md#es-specials).

**EE kernel calls**: BIOS services reached through `syscall` stubs (`FlushCache`, `AddIntcHandler`, `CreateThread` ...). See [Runtime libraries: The EE kernel calls](architecture/runtime-libraries.md#the-ee-kernel-calls).

**Effect (font)**: Glyph drawing mode: plain, shadow, outline or bold, set by code 13 or `$bg`. See [Fonts and text: Drawing one glyph](libraries/fonts-and-text.md#drawing-one-glyph).

**Effect bank**: Any bank that is not a song bank. One program, keys with `mode` 2, low sample rates. See [Sound files: Bank names](formats/sound.md#bank-names).

**Effect command (`CfeEffect`)**: A sef effect with transform tracks and an action track (spawn, flag, kill, respawn). See [Cutscenes: Effects](scripting/cutscenes.md#effects).

**Effect depth (FX volume)**: `korgEvagSetFxVolume(1, level)`, set per song by `seBgmSetSongLevel`. Most likely the reverb. See [Audio: Sequenced music](architecture/audio.md#7-sequenced-music).

**Effect family**: One kind of MEf effect: a constructor plus up to four callbacks in one source file. See [Battle effects: The effect families](game/battle-effects.md#the-effect-families).

**Effect file (.esd)**: A chunk file in `simajiri\esd` with one effect: a script table and usually its own images, colour tables, models and texture blocks. See [Effect data files: the .esd files](formats/effects.md#the-esd-files).

**Effect group of a technique**: The high part of the effect key, chosen by `tecEffectGroupGet` from the technique's `gateFlag` and the unit's `sequenceIndex`. See [Battle effects: From a command to a visible effect](game/battle-effects.md#from-a-command-to-a-visible-effect).

**Effect heap**: The `sm` allocator block `sefDefaultMemoryBlock` of 0x152800 bytes that holds effect file buffers. See [sef: The resource record table](libraries/sef.md#the-resource-record-table).

**Effect id list**: Three technique numbers, tried in order against the state of the target. Named by a negative key row entry. See [Key rows and effect id lists](formats/game-tables.md#key-rows-and-effect-id-lists).

**Effect index (effect group)**: The 10-bit group number of an effect key. One `.esp` file holds all effects of one index. See [sef: Layout of a key](libraries/sef.md#layout-of-a-key).

**Effect key**: `kind << 28 | index << 12 | sub`, made from a nine-character effect name by `srsEffectNameToID` (`pe`, `ce`, `ee`). See [Files and resources: Effect files](architecture/files-and-resources.md#effect-files). See also [sef: Layout of a key](libraries/sef.md#layout-of-a-key). See also [sdv: The effect request](libraries/sdv.md#the-effect-request).

**Effect kind**: The two-bit family of an effect key. 0 is `pe` (battle), 1 is `ce` (field and cutscene), 2 is `ee`. See [sef: Layout of a key](libraries/sef.md#layout-of-a-key).

**Effect mode**: one of the seven `GifEffectMode` values selected with `gifStateSetMode`. Its draw function runs in callback slot 10. See [Screen effects: The status block](libraries/screen-effects.md#the-status-block).

**Effect pack (.esp)**: A chunk file in `simajiri\esp` with the images, models and embedded `.esd` files of one character, enemy type or field area. See [Effect data files: the .esp effect packs](formats/effects.md#the-esp-effect-packs).

**Effect package**: The always-loaded file `esp/default.esp` (battle) or `esp/cfdef.esp` (field) in record 0 of the resource table. See [sef: The effect package](libraries/sef.md#the-effect-package). See also [Effect data files: the .esp effect packs](formats/effects.md#the-esp-effect-packs).

**Effect pause value**: A 16-bit value (`ov01EffectPauseSet`, `ov01EffectPauseGet`) that freezes the update and advance callbacks of all families when not 0. The scene loop only ever writes 0. See [Battle effects: Pause and the tick counter](game/battle-effects.md#pause-and-the-tick-counter).

**Effect program**: The VU1 program at `svEffectMicrocodeData` that draws sprites, effect polygons, ribbons and particles. See [pxy: The effect program](libraries/pxy.md#the-effect-program).

**Effect push and pop**: `sefPushEffect` and `sefPopEffect` save and re-create the persistent field effects around a teardown. See [sef: Saving and restoring the effects](libraries/sef.md#saving-and-restoring-the-effects).

**Effect record (scheduler)**: One running effect, a `SefEffectRecord`. It holds 32 particle lines and six sequence blocks. See [sdv: How the parts fit together](libraries/sdv.md#how-the-parts-fit-together).

**Effect record (`SefEffectRecord`)**: One running effect, 0xA70 bytes, 64 in `sefEffectRecords`. The X1 name was scheduler, and several function names still say so. See [sef: Effect records](libraries/sef.md#effect-records).

**Effect request**: The block that names the performer, target, partner and child actors of an effect and its effect key (`struct EffectRequest`, read as `SefRequest` and `SefLinkedContext`). See [sdv: The effect request](libraries/sdv.md#the-effect-request). See also [Screen effects: The cutscene effect requests](libraries/screen-effects.md#the-cutscene-effect-requests).

**Effect request (`EffectRequest`)**: 0x38-byte block the battle sends to `sefCreateEffect`: source, partner, target, part actors, key, hit data, mode. See [sef: Requests from battle](libraries/sef.md#requests-from-battle). See also [Battle effects: From a command to a visible effect](game/battle-effects.md#from-a-command-to-a-visible-effect).

**Effect row**: The 7-word row that creates a coded battle object (`MSeFire`). See [Effect data files: effect rows](formats/effects.md#effect-rows).

**Effect scene flags**: `svSceneFlags`, a flag word of the effect system with four accessors. See [Events: The effect scene flags](scripting/events.md#the-effect-scene-flags).

**Effect script**: The table block that describes one effect. Entries point at each other with 16-bit word offsets. See [sef: Effect scripts](libraries/sef.md#effect-scripts-sc).

**Effect script table**: The data of a script chunk: 16-bit words holding tables linked by word offsets. See [Effect data files: the effect script table](formats/effects.md#the-effect-script-table).

**Effect sequence**: The sequence that creates `MSe` battle effect objects through `MSeFire` (record field `sound`). See [sdv: The effect sequence](libraries/sdv.md#the-effect-sequence).

**Effect slot (resource)**: Records 9 to 56 of the resource table, each holding one `.esd` file, evicted by least recent use. See [sef: Effect slots](libraries/sef.md#effect-slots).

**Effect step**: One call of `sefExecStep`: scheduler pass, alters, emitters, signal clear. See [sdv: The per-frame order](libraries/sdv.md#the-per-frame-order).

**Effect type**: Number written by `srsAnalyzeEftNo`: 0 package effect, 1 `.esp` effect, 9 `.esd` effect. See [sef: File kinds](libraries/sef.md#file-kinds).

**Elevator**: One of eight `ElevatorRecord` positions moved by `id.elvMove`, with floor triangles of attribute `0x200 + n` and a carrying model unit. See [Maps and units: Elevators](game/map-and-units.md#elevators).

**eligibility bitmap**: 256 bits in the progress record, one per special item ever held. See [Party and growth: The boxes](game/party-and-growth.md#the-boxes).

**Embedded .esd**: A kind-10 chunk of a pack that wraps a whole `.esd` file. The script table starts at chunk offset 0xC0. See [Effect data files: embedded .esd chunks](formats/effects.md#embedded-esd-chunks).

**Emission shape**: Random shape for the start and target position of a point: two spheres, three boxes, two circles. See [sef: Emission shapes](libraries/sef.md#emission-shapes).

**Emitter**: A voice record with a world position. Several emitters may share one voice, the loudest wins. See [Audio: Looping sounds](architecture/audio.md#9-looping-sounds-and-positional-sound). See also [sdv: Slots, emitters and the particle pool](libraries/sdv.md#slots-emitters-and-the-particle-pool).

**Emitter track**: One of the seven keyed tracks of a line: start position, rotation, target position, scale, direction, points per emission, emission interval. See [sef: Lines](libraries/sef.md#lines).

**Emitter track table**: Seven offsets of a line: start position, start shape extent, target position, target shape extent, direction, points per emission, emission interval. See [Effect data files: the emitter track table](formats/effects.md#the-emitter-track-table).

**Empty caption**: An entry with a stamp and no text, which clears the caption. See [section 3.2](formats/movies-images-and-icons.md#32-the-text-format).

**Empty entry**: A 16-byte entry with the range 100.0 to -100.0. It keeps the motion numbers of a pack fixed and is refused when bound. See [Skeletons and motion: Entries](formats/skeletons-and-motion.md#entries).

**Encounter reaction**: `gameWork.encounterReaction`, -1 normally. Any other value makes `Game()` start a battle. See [Field: Battles started from the field](game/field.md#battles-started-from-the-field).

**End (opcode 0x1C)**: Stops a thread for the frame and rewinds it to its entry. The thread is not released. See [The SB virtual machine: One thread](scripting/sb-vm.md#one-thread).

**`.ene` file**: A table of 65 behaviour presets of `0x30` bytes for field enemies and NPCs. See [Maps and field data: Field character presets](formats/maps-and-field-data.md#field-character-presets-ene).

**Enemy group**: One formation of an encounter: up to five spawn records. See [Enemy groups](formats/game-tables.md#enemy-groups).

**Enemy preset file (`.ene`)**: 65 records of 48 bytes (`EnepcParamSet`) with size, speeds, field of view and timers for field enemies and people. See [Enemy preset files (.ene)](formats/scripts-and-archives.md#enemy-preset-files-ene).

**`Enemy_ActionReady`**: Starts an action: stores it, resets the timer, sets the motion and the action length. See [Field enemies: Starting an action](game/field-enemies.md#starting-an-action).

**`Enemy_After_Battle`**: Calms all enemies: ends stuns and sends chasers home. Called after a battle, during the grace period, on a ladder grab. See [Field enemies: Calming the others](game/field-enemies.md#calming-the-others).

**`Enemy_Init`**: Prepares a character for its first action and decides by resource id (`0x18000` to `0x1901E`) whether it is an enemy. See [Field enemies: Enemy_Init](game/field-enemies.md#enemy_init).

**enemyorder.dat**: 513 rows of 16 bytes: order number, two gate bytes, enemy model id, eight monster set numbers. See [Maps and field data: enemyorder.dat](formats/maps-and-field-data.md#enemyorderdat).

**`enemyorder.dat` (`uwamonoKeyRowTable`, `UwamonoKeyRow`)**: Table of placed enemies, 16-byte rows: key, two respawn gate bytes, model resource id, eight monster set numbers. See [Field enemies: Data files](game/field-enemies.md#data-files).

**enepc**: Module of 18 natives that configure field enemies and NPCs. See [Native functions: The enepc module](scripting/natives.md#the-enepc-module). See also [Field enemies: Enemies, NPCs and the player](game/field-enemies.md#enemies-npcs-and-the-player).

**Entry block**: `dataEntryBlock`, the single file table for the extra model and motions of a technique. See [The battle engine: The entry block](game/battle-engine.md#the-entry-block).

**Entry flag 1 (hit actor)**: Flag on the source actor that an effect slot sets when the blow lands. It releases the reaction of the targets. See [Battle effects: Entry flags](game/battle-effects.md#entry-flags).

**Entry flag 4 (show hit)**: Flag on the target actor that makes `attackHitsShow` pop the damage number. See [Battle effects: Entry flags](game/battle-effects.md#entry-flags).

**Entry flags**: Flag word per actor entry that effect slots set and the battle reads (`sefGetEntryFlags`, `sefIsHitActor`). See [sef: Slots](libraries/sef.md#slots).

**Entry group**: The number 0 to 5 in a direct entry's key that selects when it is played. See [Rendering: Direct entries](architecture/rendering.md#direct-entries). See also [pxy: Direct sends](libraries/pxy.md#direct-sends).

**Entry key**: `gameWork.entryKey`, the key of the map entry point the player is put on in the new place. -1 none, -2 the stored party position. See [Events: Place numbers](scripting/events.md#place-numbers). See also [Field: The frame loop](game/field.md#the-frame-loop).

**Entry mask**: Sub pilot bit mask of an E.S. special, bit `0x8000 >> (key - 1)`. See [E.S. specials](formats/game-tables.md#es-specials).

**Entry number (`keepId`)**: The number of an entry inside its group, the key of `UnduDataGetHeader`. See the same section.

**Entry point**: A row (`UwamonoEntry`: position, angle, key) of group `0x0300`, where a place puts the player or a script places a thing. See [Maps and field data: Entry points](formats/maps-and-field-data.md#entry-points-group-0x0300).

**Entry table**: `resEntryTable`, 64 words that the command handlers fill. Each word is a file id or a kind plus an offset into the name pool. See [Events: The entry table](scripting/events.md#the-entry-table).

**Entry table (`resEntryTable`)**: The sorted list of up to 63 words built from a scene resource script: file ids, effect names, sound records, events. See [Files and resources: Reading the list](architecture/files-and-resources.md#reading-the-list).

**`.epf` file**: The node table of a model, magic `EPF `. See [Maps and field data: Node tables](formats/maps-and-field-data.md#node-tables-epf).

**Episode I bonus**: 30 Skill Points per saved level for six characters, skill `0x3F` for Ziggy and KOS-MOS, special item `0x45`. See [Party and growth: The bonus for Episode I save data](game/party-and-growth.md#the-bonus-for-episode-i-save-data).

**Equip slots (entries)**: Three (or four) slots of a character holding board record indices, mirrored in `StatBlock.effectCode`. For an E.S. they hold accessory ids. See [Party and growth: Equipping](game/party-and-growth.md#equipping).

**Equipment slots**: The 3 or 4 skills a character has equipped, or the 3 accessories of an E.S. (`PartyTakenBits.entries`). See [Main menu: Status and equipment](game/main-menu.md#status-and-equipment).

**.esd file**: Effect file with one effect, loaded into an effect slot (`esd/ce000_051.esd`). See [sef: Effect slots](libraries/sef.md#effect-slots).

**.esp file**: Effect file with all effects of one index, loaded into a wait slot (`esp/pe005.esp`). See [sef: Wait slots](libraries/sef.md#wait-slots).

**Ether (battle lookup)**: An ether record `EtherRecord` (1 to 59 single ethers, 60 to 89 combined ethers). See [Battle: Ether](game/battle.md#ether).

**Ether (lookup table)**: A record of `etherRecordGet` (109 records) with a technique and an EP cost. Learned ethers are the `codeBits`, set by the misleadingly named `dataSkillLearnSet`. See [Party and growth: Skills and ethers in the source names](game/party-and-growth.md#skills-and-ethers-in-the-source-names).

**Ether record (battle lookup)**: 12-byte record: texts, flags, technique (670 plus the ether number), two component ethers. See [Ethers](formats/game-tables.md#ethers).

**Event flags**: `gameWork.eventFlags`, 512 bytes, one per event table row. 1 means the item was picked up. See [Events: Pickup and enemy flags](scripting/events.md#pickup-and-enemy-flags).

**Event id**: the Episode I number of a place or cutscene (`cfNNNN.txt` or `evNNNN.txt`). See [From Episode I to Episode II: Events and places](architecture/from-episode-i.md#events-and-places).

**Event queue**: `eventQueueTable`, 64 records that `cmdExecute` fills for one command and `eventQueueRun` works off with 21 handler kinds. See [Battle: Executing a command](game/battle.md#executing-a-command).

**Event slot**: An index into `uwamonoEventIds`. The script stores a thread value there with `runtime.message` and C code sends to it with `uwamonoSendEvent`. See [Events: Events sent to scripts](scripting/events.md#events-sent-to-scripts). See also [Battle: The Event Slot](game/battle.md#the-event-slot).

**Event Slot panels**: the pictures CTC, BST, SKL, ETR, T·C, NON, RV of the codes `&09` to `&15`. See [Text and fonts: The picture codes of battle text](formats/text-and-fonts.md#the-picture-codes-of-battle-text).

**Experience countdown (expCounter / next_level_exp)**: Experience still missing for the next level. `calcLvUp` subtracts gains and levels up at zero. See [Party and growth: Levels and experience](game/party-and-growth.md#levels-and-experience).

**Experience table**: 99 words, entry n = 10 × (n + 1)². See [Experience table](formats/game-tables.md#experience-table).

**Extra entries**: Record indices `0x70` to `0x76`: skill `0x3F` and Shion's six ethers `0x34` to `0x39`, which exist outside the board. See [Party and growth: Extra entries](game/party-and-growth.md#extra-entries).

**Extra stream (movie)**: The second Sofdec stream created only when the Dolby Digital option is on. See [Audio: Movie sound](architecture/audio.md#18-movie-sound).

## F

**Face entry (FAC)**: An entry of the face pack, magic `FAC `, version 1.4, with only a face stream of 6 blocks. See [Skeletons and motion: The face motion pack (FAP)](formats/skeletons-and-motion.md#the-face-motion-pack-fap).

**Face motion (`.fap`)**: A motion table whose entries drive the face nodes through state B. See [Animation: Motion tables and entries](libraries/animation.md#motion-tables-and-entries).

**Face motion pack (FAP)**: `motion\face_all.fap`, magic `FAP `, the one pack of 19 face entries shared by all characters. See [Skeletons and motion: The face motion pack (FAP)](formats/skeletons-and-motion.md#the-face-motion-pack-fap).

**Face node (skip node)**: A `TiHrcSkip` record of 0x70 bytes, a bone of the face that the body solver skips and the face pack drives. Files that have them have exactly 6. See [Skeletons and motion: Face nodes](formats/skeletons-and-motion.md#face-nodes).

**Face table (resource `0x30041`)**: The shared table of face animations stored in `rsrc[XAct_Rsrc_SlotTableB]`. The battle falls back to `motion\face_all.fap`. See [Actors: The resource set](libraries/actors.md#the-resource-set).

**Face test**: Software back-face removal per triangle, on when the style level of the part is not zero. The sign in 0x3c6 chooses the side. See [pxy: The geometry pass](libraries/pxy.md#the-geometry-pass).

**Face vector**: Optional fourth vertex array of rigid batches that holds the negated unit normal of the triangle ending at the vertex. See [Vertex batches](formats/models-and-textures.md#vertex-batches).

**Fade band**: `renderFadeBand`, a soft-edged additive band drawn in five passes. See [Screen effects: Sprites and the fade band](libraries/screen-effects.md#sprites-and-the-fade-band).

**Fade colour**: `fadeState.color`, the colour that fade in and fade out move to and from. See [Screen effects: State](libraries/screen-effects.md#state).

**Fade controller**: `fade_ctrl.c`, which animates the global fade colour over a number of frames. See [Screen effects: The fade controller](libraries/screen-effects.md#the-fade-controller).

**Fade metric**: `pxyFadeMetric`, a measure of the size of a model on screen used by the distance fades and the shadow fade. See [pxy: Fading by size](libraries/pxy.md#fading-by-size).

**Fade quad**: effect mode 4, a sheared ghost of the screen. See [Screen effects: Mode 4: fade quad](libraries/screen-effects.md#mode-4-fade-quad).

**Field camera**: The camera code of `game_camera.c` with its state in `gameWork.camera`. See [Field: The field camera](game/field.md#the-field-camera).

**Field native**: A native of the main executable that targets a map object record by key or the player's record. See [Native functions: Field natives and battle natives](scripting/natives.md#field-natives-and-battle-natives).

**Field program**: `xglMode3MicrocodeProgram`, the lit program of field mode, 2038 of 2048 pairs. See [pxy: The model programs](libraries/pxy.md#the-model-programs).

**Field set**: The 15 owners and 200 natives that `scriptRegisterModules` registers for map, event and cutscene scripts. See [Native functions: The field registration](scripting/natives.md#the-field-registration).

**Field stream**: One of the 25 short `CF_<map>_<nn>.adx` files in `sound\adx\`. See [Sound files: The streams on the disc](formats/sound.md#the-streams-on-the-disc).

**Field-of-view scale**: The factor at 0x339 that multiplies S, T and Q together, which changes only the mipmap level the GS picks. See [pxy: The geometry pass](libraries/pxy.md#the-geometry-pass).

**FifoRing**: A ring of indices with read index, write index and count, used for the list of commands that wait for execution. See [The battle engine: Small containers](game/battle-engine.md#small-containers-in-obj_slotc).

**File group (`CdFileGroup`)**: A base id, a count and a name list whose entry 0 is the directory. `xglCdFileGroups` has 14 groups. See [Files and resources: File ids and file groups](architecture/files-and-resources.md#file-ids-and-file-groups).

**File id**: A number that names a resource: bits 16 to 23 are the kind, the rest selects a name in a file group (`0x10002` = `model\char\pc\shion00`). See [Files and resources: File ids and file groups](architecture/files-and-resources.md#file-ids-and-file-groups).

**File key**: The text `"%08x.%08x"` (start sector, sectors) that the game gives ADX and Sofdec instead of a file name. See [Audio: ADX streams](architecture/audio.md#11-adx-streams).

**File kind**: Class of an effect key from `srsGetEffectFileKind`: none (inside the package), `.esp` or `.esd`. See [sef: File kinds](libraries/sef.md#file-kinds).

**File set**: The files loaded together for one file id: `.xap`, `.epf`, `.xhr`, `.txy`, `.pxy` for a character or map object, `.bin`, `.sbl`, `.id`, `.txy`, `.pxy` for a map. See [Files and resources: File sets](architecture/files-and-resources.md#file-sets).

**Filler code 7**: The no-operation control code that caller layers write into the spare byte of a consumed two-byte marker. See [Fonts and text: All escapes at a glance](libraries/fonts-and-text.md#all-escapes-at-a-glance).

**Final packet callbacks**: The 17 function slots at `sRender` +0x24 called in order after the models and before the text. See [Rendering: Final packet callbacks](architecture/rendering.md#final-packet-callbacks). See also [xgl: Final packet callbacks](libraries/xgl.md#final-packet-callbacks).

**Final packet pass**: `xglRenderFinalPacket`, the loop over seventeen callback slots at `sRender.final_packet_callback` that runs after the models and before the text each frame. All screen effects draw from it. See [Screen effects: Where the layers run in a frame](libraries/screen-effects.md#where-the-layers-run-in-a-frame).

**Fire entry (`MSeFireEntry`)**: The 0x40-byte request of an MSe object: type, owner, link and target actors with their attach points, an offset and the effect id. See [Battle effects: The fire entry](game/battle-effects.md#the-fire-entry).

**Fire entry table**: `MSeFireCallbacks` (14 constructors) and `MSeFireSpawnCount` (objects per call) in `src/ov01/m_gs.c`. See [Battle effects: MSeFire and the fire entry table](game/battle-effects.md#msefire-and-the-fire-entry-table).

**First thread**: The thread that starts at the header `entry` word. It sets the place up and ends itself. See [The first thread](formats/scripts-and-archives.md#the-first-thread).

**Fixed camera**: Camera type 1, position from the record, optionally turning to an anchor. See [Field: The fixed camera](game/field.md#the-fixed-camera).

**Flag window**: Fire type 6: a pulsing tube of sixteen dark red lines along a curve. See [Battle effects: Flag window (type 6)](game/battle-effects.md#flag-window-type-6).

**Flush**: `xglFontFlush`, which re-uploads the font texture, interprets all queued records and resets the queue. Runs once per frame in the render step and on demand in menu code. See [Fonts and text: The flush](libraries/fonts-and-text.md#the-flush).

**Focus offset (damp offset)**: A direction 0 to 8 that `cfeDampDirOffset` eases the actor's focus pair towards. Likely the eye direction. Same section.

**Focus target (aim target)**: A world point channel A aims at for one frame (`XenoActorSetFocusTarget`). See [Actors: Other channel settings](libraries/actors.md#other-channel-settings).

**Focus-driven track**: Track whose value is `pxyFocusX` or `pxyFocusY` instead of its keys. See [Tracks](formats/models-and-textures.md#tracks).

**Fog override**: Fog distances and colour for effects, set with `sefSetFogOverride`. See [sef: Fog](libraries/sef.md#fog).

**Follow camera**: Camera type 0, an orbit at fixed angles around the player. See [Field: The follow camera](game/field.md#the-follow-camera).

**Follow trail**: An alter with a nonzero follow value: a waving tail of at most 32 points behind one part. See [sdv: Follow trails](libraries/sdv.md#follow-trails).

**Follower (parent, breadcrumb ring)**: A character with `record->parent` set by `objx.setParent` that follows its parent along a ring of 128 recorded positions. See [Field enemies: Following another character](game/field-enemies.md#following-another-character).

**Font file**: `font0.us` or `font1.us`, 0x78340 bytes: two image blocks of 0x3C020 bytes and a width table of 0x300 bytes. See [Text and fonts: File layout](formats/text-and-fonts.md#file-layout).

**Font module (xglFont)**: The part of xgl in `src/main/xgl_font.c` that queues text records and turns them into glyph sprites once per flush. See [Fonts and text: The layers of text drawing](libraries/fonts-and-text.md#the-layers-of-text-drawing).

**Font object (FontObject)**: An animated rectangle of four tracks (position, size and their animated twins) with open and close phases. See [Fonts and text: Objects, opening and closing](libraries/fonts-and-text.md#objects-opening-and-closing).

**Font object layer**: `src/main/font_obj.c`, the cell-based 2D picture drawer plus tracks, boxes and text blocks. See [Fonts and text: Font objects](libraries/fonts-and-text.md#font-objects).

**Font object sheet**: one of the `yosikawa\*.xtx` files, a 512 by 512 texel view with its colour tables inside the image. See [Text and fonts: Font object sheets](formats/text-and-fonts.md#font-object-sheets).

**Font state (FS)**: The static settings block of the font module: flags, line height, default width, wrap width, ruby and compact sizes, queue cursor. See [Fonts and text: The font state](libraries/fonts-and-text.md#the-font-state).

**font0.us / font1.us**: The two font files. Font 1 is the normal font, font 0 is loaded for cutscenes and movies. See [Fonts and text: Font files and loading](libraries/fonts-and-text.md#font-files-and-loading).

**FontDrawItem**: The draw request of `fontObjDraw`: cell, position, offset, size, flags. `packetRequest` is the shared instance of the menus. See [Fonts and text: Drawing a cell](libraries/fonts-and-text.md#drawing-a-cell).

**FontTrack**: A pair of values that eases toward a target, used for every moving part of the menu. See [Main menu: Tracks](game/main-menu.md#tracks).

**Foot mode**: The menu form for the party on foot (`ptState.splitMode == 0`, `menuCopilotModeFlag == 0`). See [Main menu: Foot mode and E.S. mode](game/main-menu.md#foot-mode-and-es-mode).

**Footstep bank**: A bank `fs_<who>_<material>` with a placeholder and two step variants. 770 names, 121 different files. See [Sound files: Footstep banks](formats/sound.md#footstep-banks).

**Footstep group**: The three-letter name of who walks (`shi`, `kos`, `cha`, `jrr`, `mom`, `zig`, `jin`, `r02`, `r03`, `r05`), found from the player's unit key. See [Audio: Footsteps](architecture/audio.md#10-footsteps).

**Footstep material**: A three-letter key (`kon`, `rai`, `gla`, `flo`, `iro`, `sto`, `car`, `woo`, `clo`, `wat`, `tur`, `ear`, `sno`, `sna`) for the ground under a walking unit. See [Audio: Footsteps](architecture/audio.md#10-footsteps). See also [Events: The commands](scripting/events.md#the-commands). See also [Maps and units: Footsteps](game/map-and-units.md#footsteps). See also [Maps and field data: Footstep materials](formats/maps-and-field-data.md#footstep-materials-group-0x0700).

**Form**: `UwamonoRecord.form`, 1.0 on foot and 5.0 in an E.S. It scales reach, markers and the encounter flag. See [Field: Model keys](game/field.md#model-keys). See also [Maps and units: The player record and its forms](game/map-and-units.md#the-player-record-and-its-forms). See also [Field enemies: Enemies, NPCs and the player](game/field-enemies.md#enemies-npcs-and-the-player).

**Frame**: The part of a thread's stack that belongs to one script function: a three-word header and the locals. See [The SB virtual machine: Locals and frames](scripting/sb-vm.md#locals-and-frames).

**Frame alpha passes**: Four full-screen sprites of `pxy_frame_alpha.c` that prepare or use the alpha channel of the frame as a mask. See [Rendering: Frame alpha passes](architecture/rendering.md#frame-alpha-passes).

**Frame boundary**: One call of `xglSleep()`. Waiting loops call it once per displayed frame. See [xgl: What this means for game code](libraries/xgl.md#what-this-means-for-game-code).

**Frame copy**: a full-screen textured sprite that copies the display page into the work page, the first step of nearly every effect. See [Screen effects: Pages and coordinates](libraries/screen-effects.md#pages-and-coordinates).

**Frame delta**: `xglRenderFrameDelta`, 1 when the last frame took more syncs than the interval. See [Rendering: Frame pacing](architecture/rendering.md#frame-pacing).

**Frame end code**: `00 00 01 B0` after every IPU frame, `00 00 01 B1` at the end of the file. See [section 4.2](formats/movies-images-and-icons.md#42-file-layout).

**Frame flags byte**: First byte of every IPU frame, copied to `IPU_CTRL` and to the DTD bit of `IDEC`. See [section 4.3](formats/movies-images-and-icons.md#43-the-frame-flags-byte).

**Frame header**: Return slot, saved frame and next frame base, the three words below the locals. See [The SB virtual machine: Locals and frames](scripting/sb-vm.md#locals-and-frames).

**Frame hook (`msgFrameHooks[0]`)**: The render hook slot that holds `msgWindowFlush` in every frame in which `msgQueueClose()` queued the message slots. See [UI windows: One frame of the message system](libraries/ui-windows.md#one-frame-of-the-message-system).

**Frame mode (`TALK_FRAME_*`)**: 0 full, 1 opening, 2 closing, 3 sized (old item notice shrinking). See [UI windows: The opening curve](libraries/ui-windows.md#the-opening-curve).

**Frame options (`pxyFlags`)**: Per-frame option word: reflection, glow, field and battle mode, global fog and so on. See [pxy: Frame options](libraries/pxy.md#frame-options).

**Frame page**: A frame-sized area of GS memory (`0x70` pages at 512 x 448). Two alternate as shown and drawn page, a third is a work page. See [xgl: The render state](libraries/xgl.md#the-render-state).

**Frame range**: `cfeRangeMin`, `cfeRangeMax` and the current frame of a cutscene. See [Cutscenes: The frame range](scripting/cutscenes.md#the-frame-range).

**Frame wait (opcode 0x1E)**: Waits a number of frames with the counter `wait[0]`. See [The SB virtual machine: Waiting](scripting/sb-vm.md#waiting).

**frameScale**: `gameWork.frameScale`, distance per frame per unit of stick deflection (0.00051 on foot, 0.001445 in an E.S.). See [Field: Moving](game/field.md#moving).

**Free request**: Flag 0x100 of an MSe object, set by `MSeObjRequestFree`. Mode 3 of the pool releases the slot. See [Battle effects: Object lifecycle and frame timing](game/battle-effects.md#object-lifecycle-and-frame-timing).

**Front member / back member**: The source's words for a character (id below 11, front slots 0 to 6) and an E.S. (id 11 or more, back slots 7 to 9). See [Party and growth: The cast and its ids](game/party-and-growth.md#the-cast-and-its-ids).

**Front-coded name**: A name record that overwrites the tail of the previous name. The count byte holds the number of bytes (low nibble) and the distance from the end minus one (high nibble). See [Front-coded names](formats/scripts-and-archives.md#front-coded-names).

**frontIndex / backIndex**: The current front slot (leader on foot) and current back slot minus 7 (leading E.S.). See [Party and growth: Order and the current slot](game/party-and-growth.md#order-and-the-current-slot).

**Frozen character**: Record with flag `0x400000` (talk) or `0x800000` whose action does not run. See [Field enemies: The per-frame update](game/field-enemies.md#the-per-frame-update).

**FX1 parameters**: Non-registered parameters (CC 99/98/6) at the start of a song that set the mode, volume, pan, delay and feedback of effect unit 1. See [Sound files: How a song is laid out](formats/sound.md#how-a-song-is-laid-out).

## G

**G.S. Campaign**: Global Samaritan Campaign, the 36 optional quests given by citizens. Official English name. The code calls it `g2path` (from the Japanese "G2 Campaign" and the GS Path device), a campaign a "path" or a "quest". See [G.S. Campaign: Official names](game/gs-campaign.md#official-names-and-the-names-in-the-code).

**g2path**: Module of 4 natives for quest and citizen flags. See [Native functions: The g2path and seg modules](scripting/natives.md#the-g2path-and-seg-modules). See also [G.S. Campaign: The G.S. Campaign screen](game/gs-campaign.md#the-gs-campaign-screen).

**Game flag (`CONTROL_SCRIPTED`)**: Bit 0x8000 of `gameWork.control`, set while a message window keeps a script in control and cleared when the window closes (unless option 0x4). See [UI windows: Opening a message](libraries/ui-windows.md#opening-a-message).

**Game flags**: The packed bit array `SaveData.scriptFlags` (256 bytes at `SaveData + 0x74`), read and written with `xglFlagsGet` / `xglFlagsSet` and the natives `runtime.getFlag` / `runtime.setFlag`. See [Events: Game flags](scripting/events.md#game-flags).

**Game over bit**: Bit `0x08000000` of `gameWork.control`, set by the script native `System(0)`. Ends the map loop and makes `Game()` call `GameOver()`. See [Boot and title: Game over and the ending](game/title-and-boot.md#12-game-over-and-the-ending).

**Game()**: The play session function in `src/main/game_data.c`. An outer loop runs once per place and an inner loop once per frame. See [Field: The frame loop](game/field.md#the-frame-loop).

**Game_Data_Pop**: Reloads the map, rebuilds every actor from its record and resumes the script after a battle. See [Field: Game_Data_Pop](game/field.md#game_data_pop).

**Game_Data_Push**: Saves camera, lights, masks, actor states and player parameters and suspends the script before a battle. See [Field: Game_Data_Push](game/field.md#game_data_push).

**`GameDiskChange(request)`**: The disc request screen. Low nibble is the disc, bit `0x10` selects the fixed buffer at `0x01000000`. See [Boot and title: The disc change screen](game/title-and-boot.md#10-the-disc-change-screen).

**`GameEnd()`**: The ending picture `end.jpg` and the offer to save clear data. See [Boot and title: The ending](game/title-and-boot.md#the-ending).

**`GameMpeg2Play`**: Blocking player of one `.sfd` movie with captions. See [Cutscenes: Playing a movie](scripting/cutscenes.md#playing-a-movie).

**`GameOver()`**: Shows `gameov1.jpg` with the music `203_game_over`, or calls `GameEnd()` on map 7057. See [Boot and title: Game over](game/title-and-boot.md#game-over).

**GameResArenaRelease**: Ends the innermost borrower by reloading every resource whose block lies in its range. See [Memory: Borrowing the resource area](architecture/memory.md#borrowing-the-resource-area).

**`GameResLoadID`**: Loads the file set of a file id into a block, or returns the block that already holds it. -1 = no files or library busy, -2 = no block. See [Files and resources: Loading by id and by name](architecture/files-and-resources.md#loading-by-id-and-by-name).

**`GameResLoadName`**: Loads one file by path into a block whose first `0x40` bytes hold the name. See [Files and resources: Loading by id and by name](architecture/files-and-resources.md#loading-by-id-and-by-name).

**GameResourceReset**: Frees every block at or above the base index in one step. See [Memory: Reset and the base index](architecture/memory.md#reset-and-the-base-index).

**GameResourceWorkAlloc**: Borrows a run of resource blocks (free or loaded) for a screen or sub-program without changing the table. See [Memory: Borrowing the resource area](architecture/memory.md#borrowing-the-resource-area).

**GameResourceWorkReload**: Reloads (synchronously) the arena's resources that lie outside the work range. See [Memory: Borrowing the resource area](architecture/memory.md#borrowing-the-resource-area).

**`GameSceneReset()`**: Clears actors, map units and fades, redraws the picture and opens the save screen (`MenuFileMain(0)`). See [Boot and title: The intermission](game/title-and-boot.md#11-the-intermission).

**GameSnapShot**: Developer screen capture to `host0:/home/xeno/work/snap/gameNNNN.jpg`. Its trigger `GameSnapShotCheck` is empty in this release. See [Save data: Snapshots](architecture/save-data.md#snapshots).

**gameWork**: The global `GameWork` block of `0x1B30` bytes that holds the state of the field session and is copied whole from the save image on load. See [Field: gameWork](game/field.md#gamework).

**`gameWork.loadStage`**: 0 for a fresh session, 1 right after a save was loaded. Also suppresses the intermission. See [Boot and title: Load Game](game/title-and-boot.md#load-game).

**`gameWork.nextMap`**: The requested next place, -1 when the current place goes on. The frame loop of `Game()` leaves when it is not -1. See [Events: The frame loop and the place change](scripting/events.md#the-frame-loop-and-the-place-change).

**`gameWork.panelValue` / `panelMode`**: The remaining frames of a timed section and its mode (0 running, 1 off). The field loop and the battle step count the value down. See [UI windows: The countdown panel](libraries/ui-windows.md#the-countdown-panel).

**Gate**: Field of an effect record. 0 hides the record and stops its update. See [sef: Effect records](libraries/sef.md#effect-records).

**Gate bytes**: `gateIfClear` and `gateIfSet` of an enemyorder row, which let a beaten enemy return. See the same section.

**Geometry**: A named 0x70-byte record of the model. Parts belong to a geometry through `matrixIndex`. Hide masks and part matrices are per geometry. See [pxy: The .pxy file](libraries/pxy.md#the-pxy-file).

**Geometry pass**: The loop that transforms, clips and projects every vertex of a batch and writes texture coordinates and `XYZF2`. See [pxy: The geometry pass](libraries/pxy.md#the-geometry-pass).

**Geometry record**: 0x70-byte record that names one piece of a model, with flags and a joint list. Pieces are hidden and shown by name. See [Geometry records](formats/models-and-textures.md#geometry-records).

**Get_Rnd**: `min + xglSRand() % (max - min + 1)`, a random integer with both ends included. See [Math: Random numbers](libraries/math.md#random-numbers).

**`GetBSplineLoop`**: Evaluates a closed cubic B-spline through a point list with the weight table of `spline.dat`. See [Field enemies: Routes and splines](game/field-enemies.md#routes-and-splines).

**gifState**: the module `src/main/gifState.c`: a queue of GS register writes plus the screen effects built on it. See [Screen effects: The gifState register queue](libraries/screen-effects.md#the-gifstate-register-queue).

**gifStatePacket**: the 128 quadword queue of A+D register writes filled by `gifStateAddReg` and sent by `gifStateFlush`. See [Screen effects: The gifState register queue](libraries/screen-effects.md#the-gifstate-register-queue).

**gifStatus**: the global `GifStatusBlock` with the parameters shared by all `gifState` effect modes. See [Screen effects: The status block](libraries/screen-effects.md#the-status-block).

**Global color**: `pxyGlobalColor`, a color that multiplies every model of the frame. See [pxy: Translucency](libraries/pxy.md#translucency).

**Global fade**: The full-screen colour sprite of `xglRenderGlobalFade`, driven by `xglRenderGlobalFadeColor`. See [Rendering: Global fade](architecture/rendering.md#global-fade). See also [xgl: The global fade](libraries/xgl.md#the-global-fade).

**Global fade colour**: `xglRenderGlobalFadeColor`, the packed colour the render module draws over each finished frame. See [Screen effects: The fade controller](libraries/screen-effects.md#the-fade-controller).

**Global fog**: Frame-wide fog set with `nmlModelSetGlobalFogDist` and `nmlModelSetGlobalFogCol`. See [pxy: Fog](libraries/pxy.md#fog).

**Globals**: The 32-bit words behind the code of an image, shared by all its threads, holding variables and constant tables. See [The SB virtual machine: Globals](scripting/sb-vm.md#globals). See also [Globals](formats/scripts-and-archives.md#globals).

**Glow model**: a model with header flag `0x100`. It is drawn a second time alone into the work page for the glow pass. See [Screen effects: The glow pass](libraries/screen-effects.md#the-glow-pass).

**Glow pass**: `gifStateDrawBlurFrame`, the three level blur of the glow models that is added to the screen, in callback slot 1. See [Screen effects: The glow pass](libraries/screen-effects.md#the-glow-pass).

**Glow source pass**: Draw mode 2, a second drawing of marked parts that the glow screen effect reads. See [pxy: One camera](libraries/pxy.md#one-camera).

**Glow test scene (`glowRunScene`)**: Developer scene that builds a rigid and an animated actor from host files. See [Actors: The glow test scene](libraries/actors.md#the-glow-test-scene).

**Glyph cell**: A 20 by 24 texel cell of the 640 by 768 font texture, 32 columns by 32 rows. See [Fonts and text: Texel layout](libraries/fonts-and-text.md#texel-layout). See also [Text and fonts: Glyph indexes 0 to 383](formats/text-and-fonts.md#glyph-indexes-0-to-383).

**Glyph index**: The number `xglFontGetGlyphIndex` derives from an EUC code. Column is `n & 31`, row `(n >> 5) & 31`, sheet `n >> 10`. See [Fonts and text: Glyph indexes](libraries/fonts-and-text.md#glyph-indexes).

**Glyph sheet**: One of two 2-bit glyph images packed into the 4-bit font texture and separated by the two colour tables at blocks 0x3C00 and 0x3C01. See [Fonts and text: Two sheets in one texture](libraries/fonts-and-text.md#two-sheets-in-one-texture). See also [Text and fonts: Two-bit sheets inside a four-bit texture](formats/text-and-fonts.md#two-bit-sheets-inside-a-four-bit-texture).

**Glyph string**: A label drawn from glyph strips of the texture with a shadow (`textDrawGlyphString`), not with the font. See [Main menu: The text and sprite layer](game/main-menu.md#the-text-and-sprite-layer).

**gr helpers**: The packet and primitive helpers of OV01 (`grGpInit`, `grPutSpr`, `grPacketSend`, `grBBCalc`). See [The battle engine: The gr helpers](game/battle-engine.md#the-gr-helpers).

**Grace period (`gameWork.fadeTimer`)**: 90 frames after a battle without contact or discovery, with the player drawn translucent. See [Field enemies: The grace period](game/field-enemies.md#the-grace-period).

**grBBCalc**: Projects the origin of an object to screen pixels, or reports it off screen. See [The battle engine: Matrices and projection](game/battle-engine.md#matrices-and-projection).

**Ground group**: Group `0x0100`, the floors. See [Maps and field data: Ground attributes](formats/maps-and-field-data.md#ground-attributes-group-0x0100).

**Ground status**: The value `UnduCheck` leaves in `actor->collision.kind`: slope, ladder or elevator class. See [Field: Slopes and moving floors](game/field.md#slopes-and-moving-floors).

**Group (of an `.id` file)**: A typed list of entries (`MapFixupGroup`). Seven types exist on the disc. See [Maps and field data: Group records](formats/maps-and-field-data.md#group-records).

**Group centre**: Average position of the targets of a group effect, kept for 600 frames in `sefGroupTable`. See [sef: Group effects](libraries/sef.md#group-effects).

**Group effect**: Effect of script kind 0x3E with one shared record and one staggered record per target. See [sef: Group effects](libraries/sef.md#group-effects).

**Group key (`sefGroupKey`)**: Counter that advances once per effect frame. Records created in the same frame share it. See [sef: The group key](libraries/sef.md#the-group-key). See also [sdv: The per-frame order](libraries/sdv.md#the-per-frame-order).

**Group list**: Array of references in the script globals, one per group, each to a pair (group table, battle map number). See [Enemy groups](formats/game-tables.md#enemy-groups).

**Group type**: The 16-bit word `0x0100` ground, `0x0200` camera surfaces, `0x0300` entry points, `0x0600` camera areas, `0x0700` footsteps, `0x0A00` light areas, `0x0B00` keep-off zones. See the same section.

**`group_slot`**: Which menu entry is shown at each of the four cursor positions of the title menu, -1 for none. See [Boot and title: The menu](game/title-and-boot.md#the-menu).

**Growth curve (GrowthRecord, unitGrowthStep)**: Eight start and target values per unit. Each level moves each parameter a fraction of the remaining distance. See [Party and growth: The growth curves](game/party-and-growth.md#the-growth-curves).

**Growth record**: Start and target of the eight growth curves of a party unit. See [Growth records](formats/game-tables.md#growth-records).

**GrPacket**: A two-word packet (data pointer, quadword count) built in the scratchpad and sent as a VIF1 DIRECT transfer. See [The battle engine: Packets](game/battle-engine.md#packets).

**GS entry, wide GS entry**: Two three-pair loops in every model program that kick the register list at 0x330 or 0x228 to the GS. See [pxy: The model programs](libraries/pxy.md#the-model-programs).

**GS flush**: `nmlPacketAddGsFlush`, which sends the direct queue through VU1 (unpack to `0x330`, `MSCAL` to the GS kick entry) so that register changes stay in order with model drawing. The wide form uses `0x228`. See [Rendering: Writing into a packet](architecture/rendering.md#writing-into-a-packet).

**GS register queue**: The immediate-mode batch layer of `gs_queue.c` (`gsQueueBegin*`, `gsQueueAdd*`, `gsQueueFlush`). See [Rendering: The GS register queue](architecture/rendering.md#the-gs-register-queue).

**GS register queue (direct queue)**: List of A+D register writes built in the packet with `nmlPacketGsInit` and sent through VU1 or VIF DIRECT. See [pxy: The GS register queue](libraries/pxy.md#the-gs-register-queue).

**GS upload chain (`makeGstransChain`)**: The DMA chain that sends a decoded frame as 16x16 macroblocks. Same section.

**gsQueue batch**: a list of GS register writes on the scratchpad started by `gsQueueBeginSprites` or `gsQueueBeginTriStrip` and sent by `gsQueueFlush`. See [Screen effects: The common record words](libraries/screen-effects.md#the-common-record-words).

**GsQueueContext**: The 0x7C-byte record that describes a queue batch and is also the draw record of the record system. See [Rendering: The GS register queue](architecture/rendering.md#the-gs-register-queue).

**Guide model (`ukun00`)**: The only 3D model of the campaign screen, shown in a camera window. Its motion mode (0 to 3) follows the page and the result. See [G.S. Campaign: The guide model](game/gs-campaign.md#the-guide-model).

**Gust**: A random factor from 0.5 to 1.19 that each body draws when a wind is at its low point. See [Animation: The push on a body](libraries/animation.md#the-push-on-a-body).

## H

**Handle table (`sefHandleSlots`)**: 48 pairs of key and buffer that record which effects the field has loaded, so they can be loaded again. See [sef: Loading by key and the handle table](libraries/sef.md#loading-by-key-and-the-handle-table).

**Handler result**: The value 0 to 3 an opcode handler returns: continue, wait, end, yield. See [The SB virtual machine: One thread](scripting/sb-vm.md#one-thread).

**Handler thread**: A named thread that polls its message queue with opcode 0x20 and ends each pass with opcode 0x1C. See [How a handler thread waits for events](formats/scripts-and-archives.md#how-a-handler-thread-waits-for-events).

**`hdd_check()`**: Stub that returns 1 in this release, which means "run the card check". See [Boot and title: The memory card check](game/title-and-boot.md#4-the-memory-card-check).

**`hdd_stage`**: The title screen's reading of `xglHddMount()`: 0 ready to install, 1 installed, 2 to 4 error kinds, -1 none. Always -1 in this release. See [Boot and title: The menu](game/title-and-boot.md#the-menu).

**HddActive / xglHddActivate**: Switch for reading the archives from the hard disk (0 off, 1 on, 2 locked, 3 error pending). See [Save data: The game partition](architecture/save-data.md#the-game-partition).

**`hddi.bin`**: Pack of five JPEG backgrounds of the HDD dialogs with a `0x40`-byte header. See [Boot and title: The dispatcher](game/title-and-boot.md#the-dispatcher).

**`HddInstallCB`**: Progress callback of `xglHddInstall`: step text, gauge, percentage, background change every 20 percent, abort dialog. See [Boot and title: The install dialog](game/title-and-boot.md#the-install-dialog).

**Header record (of a track)**: One or two key-sized records in front of the first key that hold constants: position kind, shape kind, random ranges, base scale, angle kind, placement kind, draw mode. See [Effect data files: key tracks](formats/effects.md#key-tracks).

**headerALL**: The fixed 623-byte JPEG header of every stream the game encodes. See [section 9](formats/movies-images-and-icons.md#9-save-thumbnails).

**Heading**: A rotation about Y measured from +Z toward +X, computed as `srsAtan2(dx, dz)`. See [Math: Axes, headings and pitch](libraries/math.md#axes-headings-and-pitch).

**Hearing (`Enemy_FindByEar`, `noticeRange`)**: A running player (stick 192 or more) is heard by every enemy within `noticeRange` with a free line through the map. See [Field enemies: Hearing](game/field-enemies.md#hearing).

**Height ramp**: The eight heights of `unitPoseRamp` (0 to 3.0) that lift a unit into the Air state and bring it down again. See [The battle engine: Hit reactions](game/battle-engine.md#hit-reactions).

**held / pressed / repeated / released**: The four button words of a pad record. The `direction*` variants add the left stick as d-pad. See [xgl: Reading](libraries/xgl.md#reading).

**Help line**: The line at the bottom of the menu that explains the item under the cursor, chosen by `menuMessageTextUpdate`. See [Main menu: The typed-out help line](game/main-menu.md#the-typed-out-help-line). See also [G.S. Campaign: The help line](game/gs-campaign.md#the-help-line).

**Hemisphere factor**: h = (N · `viewDirection` + 1) / 2, the weight between the ambient color and the lower ambient color. See [pxy: Lighting](libraries/pxy.md#lighting).

**Hermite curve**: A cubic curve between two points with a tangent at each end, `MMathCalcHermite`. See [Math: Hermite curves](libraries/math.md#hermite-curves).

**Hermite key track (MMathHermiteKey)**: A list of points with a 16-bit time in the w slot, ended by time `0x400`, evaluated by `MMathEvalHermiteKeys`. See [Math: Hermite curves](libraries/math.md#hermite-curves).

**Hermite path**: Fire type 1: eight thin blue trails on random curves out of an attach point. See [Battle effects: Hermite path (type 1)](game/battle-effects.md#hermite-path-type-1).

**Hidden viewer mode**: Three quick full turns of the right stick hide the title picture and menu. The left stick then turns the camera and R3 keeps the mode alive. See [Boot and title: The hidden viewer mode](game/title-and-boot.md#the-hidden-viewer-mode).

**Hide mask**: One bit per geometry given with `pxySetHideMask`. A set bit hides the parts of that geometry. See [pxy: Submitting a model](libraries/pxy.md#submitting-a-model). See also [Maps and units: Drawing the map](game/map-and-units.md#drawing-the-map).

**Hide mask (`visibility`, `subVisibility`)**: One bit per model geometry. A set bit hides the part. See [Actors: Visibility](libraries/actors.md#visibility).

**Hierarchy (`.xhr`, `TiHrcData`)**: The node list of a model with rest pose, dynamics nodes, collision shapes and link table. Version at least 2.3. See [Animation: The hierarchy data](libraries/animation.md#the-hierarchy-data).

**Hierarchy (XHR)**: The `.xhr` file of a model, magic `XHR `, version 2.3 or 2.4. It lists the nodes of the skeleton in seven groups with their rest pose, plus body collision shapes and a link table. In memory it is `TiHrcData`. See [Skeletons and motion: Hierarchies (XHR)](formats/skeletons-and-motion.md#hierarchies-xhr).

**Highlight pulse**: `pulse_value`, the alpha of the cursor bar, bouncing between 0x10 and 0x45 in steps of 3. See [UI windows: Cursor, sweep and scroll](libraries/ui-windows.md#cursor-sweep-and-scroll).

**Hit effect**: Effect group 0x136 (records 0x11 to 0x14) that `attackHitsShow` starts on a target at the moment of impact. See [Battle effects: Other senders](game/battle-effects.md#other-senders).

**Hit kind**: The low byte of `HitRecord.hitMode` (0 miss, 1 HP loss, 2 HP gain, 3 EP loss, 4 EP gain, 5 status, 6 cure, 7 to 10 drains). See [Battle: Hit and evade](game/battle.md#hit-and-evade).

**Hit record**: `struct HitRecord`, the computed outcome of one technique on one target. See [Battle: Hit and evade](game/battle.md#hit-and-evade).

**Hit roll**: The two rolls of `calcHitRate` (miss, then guard) against the technique's two thresholds. See [Battle: The hit roll](game/battle.md#the-hit-roll).

**Hit signal (`_hitSignal`)**: A flag raised by a particle slot with flag bit 0 at the moment of impact and cleared at the end of each effect step. See [sdv: The hit signal](libraries/sdv.md#the-hit-signal).

**Hit wait row**: A row with bit `0x1000` in its frame. It fires on the hit signal instead of at a frame. See [sdv: The key test](libraries/sdv.md#the-key-test).

**Hokan entry**: A `CameraKeyEntry` of `cameraSlotTable`, the transition for a change from one record to another. See [Field: CameraRange and the camera records](game/field.md#camerarange-and-the-camera-records).

**Hold flag (`XenoActorHoldFlag`)**: Global switch that stops all actor updates, used by the cutscene pause. See [Actors: The update each frame](libraries/actors.md#the-update-each-frame).

**Hover (unit move)**: A move in which the unit fades out, is placed at the goal and reappears. See [The battle engine: Moving](game/battle-engine.md#moving).

**Hundredths**: The fixed scale of script numbers: positions times 100, angles in hundredths of a degree. See [Native functions: Arguments on the stack](scripting/natives.md#arguments-on-the-stack).

## I

**Icon code (`&NN`)**: `&` and two digits inside menu help text (01-04, 10-12) or battle text (01-29), replaced by a picture. See [Fonts and text: The help line and its icons](libraries/fonts-and-text.md#the-help-line-and-its-icons) and [textDrawString and its icon codes](libraries/fonts-and-text.md#textdrawstring-and-its-icon-codes).

**Icon shape**: One complete set of vertex positions of a console icon. See [section 8.3](formats/movies-images-and-icons.md#83-the-3d-icon-format).

**Icon texture run-length code**: 16-bit code of a compressed icon texture: below `0xFF00` a repeat count, otherwise a count of literal pixels. See [section 8.3](formats/movies-images-and-icons.md#83-the-3d-icon-format).

**icon.sys**: The 964-byte `PS2D` description of a save (title, colours, lights, icon names). See [section 8.2](formats/movies-images-and-icons.md#82-iconsys).

**id (module)**: The owner name of the 7 elevator, slope, keep-off and jump point natives. See [Native functions: The id module](scripting/natives.md#the-id-module).

**`.id` file**: The collision and layout file of a map, "ID data" in the code's message, read by the `Undu*` functions. See [Maps and field data: Collision and layout data](formats/maps-and-field-data.md#collision-and-layout-data-id).

**im.bin**: Archive of the four intermission pictures. See [section 6.2](formats/movies-images-and-icons.md#62-imbin).

**Image**: A script file in memory: header, 16-bit code, 32-bit globals, name table and imports. Its address is also the key of its threads. See [The SB virtual machine: The image](scripting/sb-vm.md#the-image).

**Image block**: a ready-made VIF1 packet piece of a font file: DIRECTHL code, GIF IMAGE tag, 0x3C000 bytes of texels. See [Text and fonts: File layout](formats/text-and-fonts.md#file-layout).

**Image key**: A kind 1 key with index 300 to 449, whose `.esp` file is loaded into wait slot 0 on the field. See [sef: Wait slots](libraries/sef.md#wait-slots).

**Image list**: The 6-word `SefAnalyzeRecord` entries of a line with an 8-word scroll header, ended by `0400 00FE`. See [Effect data files: the image list of a line](formats/effects.md#the-image-list-of-a-line).

**Image mapper**: List of the images, palettes, models and scripts of one loaded effect file. 57 lists exist in `_imageMapper`. See [sef: Registering a file](libraries/sef.md#registering-a-file).

**Image mapper (`SvTypeList`)**: Catalogue of the chunks of one effect file: images, palettes, scripts, models. 57 exist. See [pxy: Image mappers](libraries/pxy.md#image-mappers).

**Image mapper id**: The effect id stored in an MSe object. Mode 2 forces it as the active image mapper so the object draws with the textures of the effect that fired it. See [Battle effects: Textures and the image mapper](game/battle-effects.md#textures-and-the-image-mapper).

**Image packet (`SefImagePacket`)**: Resolved image of a line: size, data, palette and a ready `TEX0` value. Up to 30 per line. See [sef: Images of a line](libraries/sef.md#images-of-a-line).

**Image reference**: A 16-bit value in a script whose low byte is a chunk id. Bit 0x0100 selects the package, 0x0200 marks a pack resource (not tested by the code), 0x2000 marks a model. See [Effect data files: image references](formats/effects.md#image-references).

**Impact triangle**: the three random points (`gifBreakPoints`) from which the screen is cut into shards. See [Screen effects: Shards](libraries/screen-effects.md#shards).

**Import**: One word of an image that names a native by owner and method name. The linker replaces it with a native id. See [The SB virtual machine: Linking](scripting/sb-vm.md#linking).

**Import list**: The list of (owner name, method name) pairs in a script image that `sbLinkImage` resolves to native ids. See [Native functions: Linking a script to its natives](scripting/natives.md#linking-a-script-to-its-natives).

**Import word**: A 32-bit word that names a native by two string offsets, owner in the high half and method in the low half. Replaced by the native id at link time. See [Imports](formats/scripts-and-archives.md#imports).

**In-place unpacking**: Packed data is read to the end of its final buffer and expanded forward. When the write position catches up, the unread stream is moved to work memory at `WorkEnd`. See [Files and resources: Packed files (ARX)](architecture/files-and-resources.md#packed-files-arx).

**InitializeSystem**: Start-up function of `control_entry.c`: IOP reboot, eight IOP modules, then all `xgl` subsystems. See [xgl: Start-up](libraries/xgl.md#start-up).

**`InitializeSystem()`**: Restarts the IOP with `IOPRP271.IMG`, loads eight IOP modules, sets `WorkEnd` to `0x00B00000` and initialises every `xgl` module. See [Boot and title: InitializeSystem()](game/title-and-boot.md#initializesystem).

**Inline command**: A `$name…,` sequence inside a string, looked up in `fontInlineCommands` during the flush (`$init`, `$col`, `$zoom`, `$prop`, `$han`, `$shadow`, `$xx`, `$ruby…`, `$mark` and others). See [Fonts and text: Inline commands](libraries/fonts-and-text.md#inline-commands). See also [Text and fonts: Inline commands](formats/text-and-fonts.md#inline-commands).

**inputDelay**: Frames until START is accepted again (16 after a pause toggle, 32 in a movie). See [Field: Pause](game/field.md#pause).

**Integer track (`CFE_TRACK_INTEGER`)**: A track of whole numbers without tangents, sampled stepwise. See [Cutscenes: The key node](scripting/cutscenes.md#the-key-node).

**Interlocked transfer**: A `.i` coprocessor move (`qmfc2.i`, `cfc2.i`, `ctc2.i`) that waits for the running microprogram to end. See [Math: Micro mode](libraries/math.md#micro-mode-and-the-resident-microprogram).

**Intermission**: The chapter picture with the offer to save, started by the `save` command. See [Events: The chapter intermission](scripting/events.md#the-chapter-intermission). See also [Boot and title: The intermission](game/title-and-boot.md#11-the-intermission).

**Interrupt registry**: The tables `xglIntcRecords` and `xglDmaRecords` with their slot maps, filled by `xglIntrInit`. See [Rendering: Interrupts](architecture/rendering.md#interrupts). See also [xgl: Interrupts](libraries/xgl.md#interrupts).

**IOP**: The console's second processor (I/O processor) with 2 MiB of its own memory. Runs the driver modules. See [Runtime libraries: IOP modules on the disc](architecture/runtime-libraries.md#iop-modules-on-the-disc).

**IOPRP271.IMG**: Replacement IOP image that `InitializeSystem` reboots the IOP with. See [Runtime libraries: Start-up](architecture/runtime-libraries.md#start-up).

**IPU movie**: A movie with the signature `ipum` decoded by the EE's IPU through `xglMovie*`. See [xgl: IPU movies](libraries/xgl.md#ipu-movies).

**IPU movie (`.ipu`)**: Silent intra-only movie with the signature `ipum`, decoded by the IPU of the console, used for monitors in cutscenes. See [section 4](formats/movies-images-and-icons.md#4-ipu-movies).

**Item flags (selectionFlagTable)**: One 16-bit word per item id that says when the item can be used in the menu and on whom. See [Main menu: The item list](game/main-menu.md#the-item-list).

**Item native (runtimeItem)**: `runtime` module native that adds or removes items of any box, teaches Double Techs, Shion's ethers, a KOS-MOS mark or skill `0x3F`, and can show the pickup message. See [Party and growth: The Item native](game/party-and-growth.md#the-item-native).

**Item notice**: Slot type 4 in `msgChoice`, the `Acquired <name>.` window after an item pickup, closed after 90 frames. See [UI windows: The item notice](libraries/ui-windows.md#the-item-notice).

**Item pickup**: Built-in type 12 and up, bound to a row of `treasurebox.dat`. Collected by walking into it. See [Maps and units: Item pickups](game/map-and-units.md#item-pickups).

**Item record (`UsableItemRecord`)**: Usable item of box 1: flags, technique 630 plus its number, field models, amount. See [Items](formats/game-tables.md#items).

**iwase**: Directory of the data files of both screens, most likely the programmer's name. See [G.S. Campaign: Data files](game/gs-campaign.md#data-files).

## J

**Java virtual machine**: the Episode I script machine in the main executable. See [From Episode I to Episode II: 5. Scripting, events and cutscenes](architecture/from-episode-i.md#5-scripting-events-and-cutscenes).

**Jitter grid**: effect mode 3, a grid with randomly shifted texture coordinates. Also the name used for record type 17. See [Screen effects: Mode 3: jitter grid](libraries/screen-effects.md#mode-3-jitter-grid).

**Joint (first, second, third group)**: A bone record `TiHrcJoint` of 0x50 bytes. The first group is the body, the second group holds extra bones (mostly roots of hair and cloth chains), the third group is empty in every file. See [Skeletons and motion: Joints](formats/skeletons-and-motion.md#joints).

**Joint attachment**: Attaching a child to an attachment node of the parent's hierarchy by id (`XenoActorAttachChildToJoint`). See [Actors: Parents, children and attachments](libraries/actors.md#parents-children-and-attachments).

**Joint base matrix**: Per-joint matrix of the model file that the actor multiplies with the global joint matrix of the skeleton. See [Matrices and joints](formats/models-and-textures.md#matrices-and-joints).

**Joint list**: The bytes at +0x30 of a geometry record: joint number plus one for each joint that skins the piece, ended by zero. Used for models with more than 0x40 joints. See [Skinning data](formats/models-and-textures.md#skinning-data).

**Joint matrices**: Skinning matrices of a model with `nJointMatrix` above zero, sent to VU1 address 0x228. See [pxy: Bones and matrices](libraries/pxy.md#bones-and-matrices).

**Joint name list (JRD)**: A text file of `#define` lines that give the matrix index of each locator and first joint by name. Two exist and no code reads them. See [Skeletons and motion: Joint name lists (JRD)](formats/skeletons-and-motion.md#joint-name-lists-jrd).

**Joint orientation (`orient`)**: A fixed rotation of a joint applied before the animated rotation, taken over from the modelling tool. See [Skeletons and motion: Joints](formats/skeletons-and-motion.md#joints).

**Joint reference**: Byte of a skinned vertex, four times the joint number (or four times the position in the joint list), 0xff when unused. See [Skinning data](formats/models-and-textures.md#skinning-data).

**JPEG backdrop**: A map `.txy` that is a JPEG picture of 1,024 by 896 pixels plus an alpha plane (`E2_MIL01`, `E2_KUK01`). See [Maps and field data: Map models, textures and scripts](formats/maps-and-field-data.md#map-models-textures-and-scripts).

**JpegWork (sw)**: Codec work area on the scratchpad at `0x70000000`, with pixel staging at `0x70003000`. See [Save data: The JPEG codec](architecture/save-data.md#the-jpeg-codec).

**`.jrd` file**: A leftover text file of the model exporter with `#define` lines for joint rows. Never loaded. See [Maps and field data: Files nothing loads](formats/maps-and-field-data.md#files-nothing-loads).

**Jump area**: Ground whose collision kind has the class `0x100`. The low five bits are the area number. Standing on it sends event 3. See [Events: Events sent to scripts](scripting/events.md#events-sent-to-scripts). See also [Maps and units: Triangles and attributes](game/map-and-units.md#triangles-and-attributes).

**Jump point (`JumpRecord`)**: An exit of the map with its position, radius and destination scene, taken from the `mapjump` function of the map script and the collision triangles of key `0x100 + kind`. See [Files and resources: Preloading](architecture/files-and-resources.md#preloading).

**Jump record / jump point**: One of 32 `resJumpRecords`: the centre and radius of a map jump area and the place it leads to. See [Events: Jump points](scripting/events.md#jump-points).

**Jump slot (SbSlot)**: A handler record (key, target, code range) for a raised error. Never filled in this build. See [The SB virtual machine: Slots](scripting/sb-vm.md#slots).

## K

**Keep-off zone**: A switchable fence mesh of group `0xB00`, controlled by `elvKeepOffFlags[32]` and `id.keepoff`. See [Maps and units: Keep-off zones](game/map-and-units.md#keep-off-zones). See also [Maps and field data: Keep-off zones](formats/maps-and-field-data.md#keep-off-zones-group-0x0b00).

**Key**: A 48-byte entry of the `KEYp` chunk: one playable sound made of two layers and a name. See [Sound files: Keys](formats/sound.md#keys).

**Key (map object key)**: The 16-bit id a script gives a map object with `runtime.setNumber` and by which all `objx`, `enepc` and `uwamono` natives find it. See [Native functions: Field natives and battle natives](scripting/natives.md#field-natives-and-battle-natives).

**Key (`TiXacKey`)**: One point of a curve: a type and flags word, a signed 16-bit frame number and a payload of 4 to 12 bytes. See [Skeletons and motion: Keys](formats/skeletons-and-motion.md#keys).

**Key buffer**: `keyBuff`, the up to eight buttons of the combination being entered. See [Battle: Button combinations](game/battle.md#button-combinations).

**Key guide**: The HUD list at (8, 312) of button sequences the current unit can enter. See [The battle engine: Key guide](game/battle-engine.md#key-guide).

**Key item bitmap**: `ptState.progress->eligibility`, one bit per key item that was ever owned. See [Main menu: Key items](game/main-menu.md#key-items).

**Key item record (`TextRecord8`)**: 107 records: 18 decoders, the G.S. Campaign objects, six robot parts, the Secret Keys. See [Keys and key items](formats/game-tables.md#keys-and-key-items).

**Key mode**: The 0 to 4 argument of `cfe.stKyCm`: plain, linear, or flat tangents on the last key. See [Cutscenes: Building a track](scripting/cutscenes.md#building-a-track).

**Key node pool**: The 6144-node pool (0x48000 bytes) all cutscene keys come from. See [Cutscenes: The node pool](scripting/cutscenes.md#the-node-pool).

**Key number**: The number 1 to 10 that a button combination gives (`keyCmdTblCha`, `keyCmdTblAgws`). It indexes the unit's key row. See [Battle: Button combinations](game/battle.md#button-combinations).

**Key range table**: `uwamonoKeyRangeTable`, ten rows of 17 words with the form change key, attack timings and effect file per model range. See [Field: Model keys](game/field.md#model-keys).

**Key row**: A row of ten technique numbers per unit in `bat.dat` (`tableKeyRowGet`). Button attacks and enemy attacks are entries of it. See [Battle: Button combinations](game/battle.md#button-combinations). See also [Key rows and effect id lists](formats/game-tables.md#key-rows-and-effect-id-lists).

**Key sequence**: A table of 16-bit rows whose first halfword is the frame at which the row fires, described by an `SdvSequence`. See [sdv: Key sequences](libraries/sdv.md#key-sequences).

**Key sequence (file side)**: One of the six row arrays of a record table: shadow, wind, camera, special, alter, effect. See [Effect data files: key sequences](formats/effects.md#key-sequences).

**Key track**: A time-sorted doubly linked list of `CfeTrackNode` keys that animates one value. See [Cutscenes: Key tracks](scripting/cutscenes.md#key-tracks). See also [Effect data files: key tracks](formats/effects.md#key-tracks).

**Key type**: Types 0, 1, 2, 3 and 5 of an XAC key, which set the key size (8, 12 or 16 bytes) and the tangent storage. See [Animation: Keys](libraries/animation.md#keys). See also [Skeletons and motion: Keys](formats/skeletons-and-motion.md#keys).

**Keyed entry**: A data item of the library, named by a script value with tag `0x200000` in bits 20 to 23, and read with `vmKeyedEntryGet`. Usually a string. See [The SB virtual machine: The library image](scripting/sb-vm.md#the-library-image).

**Keyed record / indexed record**: The 32-byte (`TiNodeKey`, version 1.1) and 28-byte (`TiNodeSlot`, version 1.0) record forms of an `.epf` file. See [Maps and field data: Layout](formats/maps-and-field-data.md#layout).

**Kick vertex**: A vertex written through `XYZF2`, which makes the GS draw the primitive. Non-kick vertices use `XYZ3`. See [Rendering: The GS register queue](architecture/rendering.md#the-gs-register-queue). See also [Screen effects: The common record words](libraries/screen-effects.md#the-common-record-words).

**Kind**: The source's name for the bank slot field of a voice id. See [Audio: Voices](architecture/audio.md#8-voices-sound-effects-as-midi-messages).

**KORG client (`korgEvag*`)**: The EE-side client of the KORG IOP sound driver, 101 functions, 30 called by the game. See [Runtime libraries: The KORG client](architecture/runtime-libraries.md#the-korg-client).

**KORG sound driver**: The third-party sound driver of Xenosaga II. Its EE client (`korgEvag*`) is linked into the executable and its IOP side is `KORGIVAG.IRX`. It plays banks, MIDI sequences and single MIDI messages. See [Audio: Layers](architecture/audio.md#2-layers).

**korgEvag**: Name prefix of the KORG client functions the game calls (30 of them). See [Audio: The KORG client calls](architecture/audio.md#5-the-korg-client-calls).

**KORGIVAG**: The KORG sound driver module on the IOP for sound effects and sequences. See [Runtime libraries: IOP modules on the disc](architecture/runtime-libraries.md#iop-modules-on-the-disc).

**Kowaremono (breakable object)**: A pack entry named in `kowaremono.dat` that breaks when struck and may release an item or an enemy. See [Maps and units: Breakable objects](game/map-and-units.md#breakable-objects).

**kowaremono.dat**: 513 rows of 48 bytes that give named map objects their size, strike level, shape, break effect and sound. See [Maps and field data: kowaremono.dat](formats/maps-and-field-data.md#kowaremonodat).

## L

**ladder**: Module of 2 natives that describe ladders. See [Native functions: The ladder module](scripting/natives.md#the-ladder-module).

**Ladder slot**: One of 16 `LadderSlot` records with a foot position, an angle, a height and up to 32 rungs. See [Maps and units: Ladders](game/map-and-units.md#ladders).

**LAN file transfer**: The unused client of `lan_usb.c` with the commands `SIZE `, `GET `, `PUT `, `SEND `, `RECV ` and `CLOSE`. See [Files and resources: File transfer over the network](architecture/files-and-resources.md#file-transfer-over-the-network).

**Language code**: The two letters returned by `xglGetLanguageCode` (`en` in the North American game), used as a directory for `.sbl` libraries and movie captions. See [Files and resources: Language directories](architecture/files-and-resources.md#language-directories). See also [xgl: Timer 0, ARX and the language code](libraries/xgl.md#timer-0-arx-and-the-language-code). See also [Fonts and text: Language handling](libraries/fonts-and-text.md#language-handling). See also [Events: Starting the place script](scripting/events.md#starting-the-place-script).

**Language directory**: `en` in `cf\en`, `model\map\en`, `movie\en`, the only language code present. See [Text and fonts: The language split](formats/text-and-fonts.md#the-language-split).

**Language library**: The SBL file `cf\<code>\scNNNN.sbl` with the strings of a place script in one language, read right behind the image and linked with `sbLibraryLink`. See [Events: Starting the place script](scripting/events.md#starting-the-place-script).

**Layer**: Group index of parts that also selects the texture set of the `.txy` file. See [pxy: Parts](libraries/pxy.md#parts). See also [Parts](formats/models-and-textures.md#parts). See also [Sound files: Keys](formats/sound.md#keys).

**Layer (alter)**: The image mapper index of the owning effect record, used to find the alter's images and to draw alters per mapper. See [sdv: Alter events](libraries/sdv.md#alter-events).

**Leash range**: Distance to the player at which a chase is given up. See [Field enemies: The chase](game/field-enemies.md#the-chase).

**Leftover bytes**: Fields that the game fills at run time, and padding, which the export tool wrote from uninitialised memory. They hold string fragments and have no stable value. See [Skeletons and motion: Conventions shared by all formats](formats/skeletons-and-motion.md#conventions-shared-by-all-formats).

**Lens**: defocus type 7, a radial stretch inside an ellipse. See [Screen effects: The types](libraries/screen-effects.md#the-types).

**Lens key**: A camera key with a perspective and a roll in degrees. See [sdv: Lens keys](libraries/sdv.md#lens-keys).

**Level, class, skill (group, slot, sub)**: The three coordinates of a skill on the board. The source names are group, slot and sub. See [Main menu: Skills](game/main-menu.md#skills).

**Levels up**: Second byte of a directory record: the number of directory levels left before the new directory is entered. See [The directory file](formats/scripts-and-archives.md#the-directory-file).

**libpkt (`sceVif1Pk*`)**: Sony's VIF1 packet building library, called from 38 game files. See [Runtime libraries: Graphics](architecture/runtime-libraries.md#graphics).

**Library (`.sbl`, `SBL `)**: The string table of one script in one language: header, index of offsets, strings. See [SBL libraries](formats/scripts-and-archives.md#sbl-libraries).

**Library image (SBL)**: The second file of a script (`.sbl`, magic `"SBL "`), which holds its keyed entries, mostly the texts in one language. See [The SB virtual machine: The library image](scripting/sb-vm.md#the-library-image).

**Library image key**: A script value with tag 0x200000 in bits 20 to 23 that names an entry of the SBL library image, resolved by `vmKeyedEntryGet`. See [Native functions: Strings, arrays and data](scripting/natives.md#strings-arrays-and-data).

**Light area**: A region of group `0xA00` that selects one of 16 light sets. Actors blend to the new set over 5 frames. See [Maps and units: Light areas](game/map-and-units.md#light-areas). See also [Maps and field data: Light areas](formats/maps-and-field-data.md#light-areas-group-0x0a00).

**Light emitter line**: Line with image flag 0x10. Its points are not drawn and light nearby actors. See [sef: Point lights from effect lines](libraries/sef.md#point-lights-from-effect-lines).

**Light key**: `XglLightKey`, the 20 current values of one light source of a pack. See [xgl: Light packs](libraries/xgl.md#light-packs).

**Light pack**: A data block with the tag `lxy` that lists animated light sources. See [xgl: Light packs](libraries/xgl.md#light-packs).

**Light pack (`lxy`)**: A file of keyed light entries, registered in a 16-slot table and turned into a light set per receiver position. See [pxy: Light packs](libraries/pxy.md#light-packs).

**Light row (`CfeLight`)**: One of four rows (ambient plus three directional lights) that point into `cfeLightSet`. See [Cutscenes: Lights](scripting/cutscenes.md#lights).

**Light set**: `XglLightSet` (`0x180` bytes): six directional lights, two ambient colours and an axis, shadow directions, three point lights. See [xgl: The light set](libraries/xgl.md#the-light-set).

**Light set (`XglLightSet`)**: The 0x180-byte lighting of one model: six directional lights, two ambient colors, a shadow direction, three point lights. See [pxy: The light set](libraries/pxy.md#the-light-set).

**Light slots**: The six directional lights of a set, chosen by strength with `xglLightSlotsInsert`. See [xgl: Light packs](libraries/xgl.md#light-packs).

**Line (`SefLocalData`)**: An emitter of 0x8B0 bytes with keyed tracks that creates points. 192 exist. See [sef: Lines](libraries/sef.md#lines).

**Line header**: The 8-word `SefLineScript`: two table offsets, gravity, floor height, bounce factor, `noAttach`. See [Effect data files: lines](formats/effects.md#lines).

**Line list**: The array of 8-word `SefSlotEntry` rows of a slot, ended by a start time of 0x400. See [Effect data files: slots and line lists](formats/effects.md#slots-and-line-lists).

**Lineage**: the relation between a subsystem of Episode I and its successor in Episode II. See [From Episode I to Episode II: What stayed, what evolved, what is new](architecture/from-episode-i.md#what-stayed-what-evolved-what-is-new).

**Linear key (`CFE_TRACK_LINEAR`)**: A key whose segment to the next key is interpolated linearly. See [Cutscenes: Sampling](scripting/cutscenes.md#sampling).

**Link table**: Cells of links between dynamics nodes with rest lengths, which keep neighbouring strands together. See [Animation: Links between nodes](libraries/animation.md#links-between-nodes). See also [Skeletons and motion: The link table](formats/skeletons-and-motion.md#the-link-table).

**linkedSlots**: Two halfwords of an E.S. parameter block: main pilot id and co-pilot id. See [Party and growth: Pilots and co-pilots](game/party-and-growth.md#pilots-and-co-pilots).

**Linking**: The one-time pass (`sbLinkImage`, `sbLibraryLink`) that resolves the imports and patches every native call in the code. See [The SB virtual machine: Linking](scripting/sb-vm.md#linking).

**List cursor (`ListCursor`)**: A 0x14-byte selection record (position, previous position, capacity, unit size, mode flags) that wraps on a fresh press and clamps on auto-repeat. See [UI windows: ListCursor](libraries/ui-windows.md#listcursor).

**List scan (MC_REQ_LIST)**: Reads every save of a card, nearest to the cursor first, and calls a callback per save. A folder without a complete `icon.sys` is reported as zeros. See [Save data: The list scan](architecture/save-data.md#the-list-scan).

**List window**: Slot type 2, lines of text with a cursor bar, used as the script menu and, with bound tables, as the sell window. See [UI windows: List windows](libraries/ui-windows.md#list-windows-script-menus).

**ListCursor**: The 0x14-byte cursor of a list: row, previous row, row count, rows per page, flags. See [Main menu: ListCursor](game/main-menu.md#listcursor).

**Load Game**: `TitleGameInit`, fade, `MenuFileMain(1)`. On 1 `loadStage = 1` and `Game()`. See [Boot and title: Load Game](game/title-and-boot.md#load-game).

**Load mode**: `srsLoadMode`: disc (`simajiri/`), host, USB or DEB. The retail game always uses the disc mode. See [sef: Load modes and paths](libraries/sef.md#load-modes-and-paths).

**Load mode (`srsLoadMode`)**: Where the effect loader reads from: CD (0), host (1), USB (2), debug file access (3). See [Files and resources: Effect files](architecture/files-and-resources.md#effect-files).

**Load queue**: Ring of 16 effect keys that found no free slot and are retried each frame, up to 300 tries. See [sef: The load queue](libraries/sef.md#the-load-queue).

**Load thread**: Short-lived EE thread of the battle overlay with a 4 KiB static stack. See [Memory: Thread stacks](architecture/memory.md#thread-stacks). See also [Battle: The load thread](game/battle.md#the-load-thread).

**Load thread (battle)**: A short-lived kernel thread (priority 8, 4 KiB stack) the battle overlay creates for one blocking load job. The only `CreateThread` in the game code. See [xgl: The load threads of the battle overlay](libraries/xgl.md#the-load-threads-of-the-battle-overlay). See also [The battle engine: The loading sequence](game/battle-engine.md#the-loading-sequence).

**`loaded_overlay`**: The number of the overlay now in the slot. `xglCdInitial` sets it to 2. See [Boot and title: The two overlays and the two discs](game/title-and-boot.md#2-the-two-overlays-and-the-two-discs).

**loadStage**: `gameWork.loadStage`, 0 for a new game, 1 and 2 while a loaded game is placed on its save point. See [Field: Starting a session](game/field.md#starting-a-session).

**Local and global matrices (`pLocalMat`, `pGlobalMat`)**: One matrix per node relative to its parent, and one relative to the model root. See [Animation: The per-frame update](libraries/animation.md#the-per-frame-update).

**Local helper copies**: Per-file copies of tiny helpers, named with their address as suffix (`xglVectorCopy_001CC998`). See [Math: Local copies](libraries/math.md#local-copies-of-the-small-helpers).

**Locals**: The stack words from `frame` upward. The arguments of a script function are its first locals. See [The SB virtual machine: Locals and frames](scripting/sb-vm.md#locals-and-frames).

**locater, kowaremono, EOB, GetItem, talk**: The other standard handler threads of a place: locators (slot 2), breakable objects (slot 1), end of battle (slot 6), item pickups (slot 8), conversations (polls `talk.check`). See [Threads and their names](formats/scripts-and-archives.md#threads-and-their-names).

**Locator**: One of the two first nodes of every hierarchy. The second one carries the movement of the body. See [Animation: Node records](libraries/animation.md#node-records). See also [Events: Locators](scripting/events.md#locators). See also [Maps and units: Locators](game/map-and-units.md#locators). See also [Skeletons and motion: Locators](formats/skeletons-and-motion.md#locators).

**Logo pictures (`credit.bin`)**: A pack of full-screen JPEG pictures shown one after the other with fade in, hold and fade out by `logoShowSlides()`. See [Boot and title: The logo pictures](game/title-and-boot.md#5-the-logo-pictures).

**`LogoFirst()`**: Runs once per boot: memory card check, then the logo pictures. See [Boot and title: The memory card check](game/title-and-boot.md#4-the-memory-card-check).

**Look link**: Fourth word of a resource block, stored in `rsrc[XAct_Rsrc_EntryTableC]` and bound as the node table of the motion package. It names points on the skeleton. See [Actors: The resource set](libraries/actors.md#the-resource-set).

**Look target**: The point an actor looks at, from the head tracks of `cfe.stKyAtHd`. See [Cutscenes: Evaluating an actor](scripting/cutscenes.md#evaluating-an-actor).

**Look-at points**: Ten map positions with a reach (`enepcLookAtPointTable`, `enepcLookAtReachTable`) defined by the long form of `enepc.lookat`. See [Field enemies: Looking at things](game/field-enemies.md#looking-at-things).

**Look-at priority**: Scheme of `Actor_LookAt_Set` / `Actor_LookAt_Release`: 3 script, 2 look-at point, 1 nearby character, 0 selected map unit. See [Field enemies: Looking at things](game/field-enemies.md#looking-at-things).

**LookupRecord (attack record)**: The 16-byte record of one attack shown on the Tech page (`attackRecordTable`). See [Main menu: Tech](game/main-menu.md#tech).

**Loop marks (song)**: The text events `loop start` and `loop end` with control change 99 values 0 and 1 next to them. See [Sound files: How a song is laid out](formats/sound.md#how-a-song-is-laid-out).

**Looping sample**: A sample with flags `0x02`, one `0x06` block (loop start) and a last `0x03` block. 308 exist, 140 of them in map banks. See [Sound files: Sample data](formats/sound.md#sample-data).

**Loose file**: A file that is not in an archive and is found in the ISO 9660 file system with `sceCdLayerSearchFile` (upper case, `,1` suffix): the overlays, IOP modules, `SYSTEM.CNF` and the `XENOSAGA.n0` directories themselves. See [Files and resources: From a path to a position](architecture/files-and-resources.md#from-a-path-to-a-position).

**`LW` (`CdReadControl`)**: The control block of the file library at `0x955C80`: active read, drive and disc state, dispatch state and the request queue. See [Files and resources: The request queue and the per-frame service](architecture/files-and-resources.md#the-request-queue-and-the-per-frame-service).

## M

**Macro mode**: Use of VU0 as coprocessor 2, one instruction at a time inside EE code. See [Math: Macro mode](libraries/math.md#macro-mode).

**Magic stamp of RgHeap**: the text `gAMe sHoW 2001.10.15` of Episode I and its first 15 characters in Episode II. See [From Episode I to Episode II: 3. Memory](architecture/from-episode-i.md#3-memory).

**`__main`**: libgcc function that `main` calls first to run global constructors. See [Runtime libraries: Start-up](architecture/runtime-libraries.md#start-up).

**Main menu**: The screen opened with TRIANGLE on the field. It runs inside one call of `menuSceneRunLoop()` with its own frame loop. See [Main menu](game/main-menu.md#main-menu).

**Main pilot**: The fixed character of an E.S. (2, 6, 7 for E.S. 11, 12, 13). The E.S. inherits the pilot's E.S.-valid skills. See [Party and growth: Pilots and co-pilots](game/party-and-growth.md#pilots-and-co-pilots).

**`main()`**: The entry of the game code in `src/main/control_entry.c`. It performs the whole start-up and then becomes the endless title loop. It never returns. See [Boot and title: From main() to the title loop](game/title-and-boot.md#1-from-main-to-the-title-loop).

**MainActive**: The header of the loaded map resource (`gameWork.active`): model, texture or JPEG, collision block, data image. See [Maps and units: The map resource](game/map-and-units.md#the-map-resource).

**MainMenu.txd**: The text table of the menu, the save screen and the result screen, read into `menuMainTxdBuffer` and indexed by `menuMainTxdEntry`. See [Main menu: Files the menu reads](game/main-menu.md#files-the-menu-reads).

**MainMenu.txd / Tutorial.txd / Location.txd**: Menu texts (569 entries), the skill tutorial script (255 lines), save screen place names (79). See [The .txd text tables](formats/game-tables.md#the-txd-text-tables).

**Map**: The 3D place of the field: one resource with a scenery model, textures and a collision block. See [Maps and units: The map resource](game/map-and-units.md#the-map-resource).

**Map (cfe) (`CfeMap`)**: A cutscene background with animated part matrices, colour scale and collision set. See [Cutscenes: Maps](scripting/cutscenes.md#maps).

**Map 10 start**: Unreachable arm of `Title()` (cursor 4) that starts `Game()` on map 10 and skips the synopsis. Very likely a developer start. See [Boot and title: The unreachable arms](game/title-and-boot.md#the-unreachable-arms).

**Map backdrop**: the large pre-rendered picture that `mapBackdropDraw` scrolls behind maps without a 3D map model. See [Screen effects: The map backdrop](libraries/screen-effects.md#the-map-backdrop).

**Map bank**: The one or two sound banks a scene names with `mapse`, bank slots 3 and 4. See [Audio: Sound banks](architecture/audio.md#6-sound-banks). See also [Sound files: Map banks](formats/sound.md#map-banks).

**Map block**: The resource block of a map: a `0x40`-byte header (`CdFileSetHeader`, read as `MainActive`) followed by `.bin`, `.sbl`, `.id`, `.txy`, `.pxy`. See [Maps and field data: The map block in memory](formats/maps-and-field-data.md#the-map-block-in-memory).

**Map name table**: `xglCdMapModelNames`, 361 names, entry n is file id `0x20000 + n`. 264 have files. See [Maps and field data: The name table](formats/maps-and-field-data.md#the-name-table).

**Map object (mobj)**: A model set `mobjNNN` of `model\mobj\`, named by the `mobj` command, file id kind `0x70000`. See [Maps and field data: Map object models](formats/maps-and-field-data.md#map-object-models-modelmobj).

**Map picture with alpha plane**: `E2_MIL01.txy` and `E2_KUK01.txy`, a 1,024 × 896 JPEG followed by one alpha byte per pixel. See [section 5.4](formats/movies-images-and-icons.md#54-map-pictures-with-an-alpha-plane).

**Map program**: `xglVertexColorMicrocodeProgram`, mode 1, selected by header flag 0x1. Vertex colors instead of lights, fog per vertex, a triangle clipper, no skinning. See [pxy: The Map program](libraries/pxy.md#the-map-program).

**Map script**: The script `cf\scNNNN.sb` of a map, loaded by `mapSceneScriptLoad` into `mapSceneScriptBuffer`. See [The SB virtual machine: Loading a map script](scripting/sb-vm.md#loading-a-map-script).

**Map set**: The up to five files of one map name in `model\map\`: `.pxy`, `.txy`, `.id`, `.bin` and `en\<name>.sbl`. See [Maps and field data: The map directory](formats/maps-and-field-data.md#the-map-directory).

**Map unit (uwamono)**: Any object placed on a map: one of the 256 records of `uwamonoRecords`. Japanese for "thing placed on top". See [Maps and units: Map units](game/map-and-units.md#map-units).

**MAP_HEIGHT_NONE**: The value -1000 that means "no ground". See [Maps and units: The probe](game/map-and-units.md#the-probe).

**MAP_ID_GAME_END**: Map `0x1B91` (7057). A save made there sets `mapFlag`, and loading it resumes on map `0x9EC` with story progress `0x75`. See [Save data: Loading into the game](architecture/save-data.md#loading-into-the-game). See also [Boot and title: Game over and the ending](game/title-and-boot.md#12-game-over-and-the-ending).

**`MAP_ID_NEW_GAME`**: Map 7001 (`0x1B59`), the first map of a new game. See [Boot and title: New Game](game/title-and-boot.md#new-game).

**`mapActiveObject`**: The record of the talk partner between the button press and the message start. See [Events: Finding the partner](scripting/events.md#finding-the-partner).

**`mapjump`**: The function name the game looks for in a place script. Its switch cases are read without running them to learn the destination of each jump area. See [Events: Jump points](scripting/events.md#jump-points). See also [Threads and their names](formats/scripts-and-archives.md#threads-and-their-names).

**mapObjectTable**: The second pack inside the scenery model, holding the movable map objects and their matrices. See [Maps and units: The map resource](game/map-and-units.md#the-map-resource).

**MapRecord**: One 16-byte collision triangle: attribute, three neighbours, three vertex indices, a plane index. See [Maps and units: Triangles and attributes](game/map-and-units.md#triangles-and-attributes).

**mapResourceSwitch**: Loads a map model by resource id. Its callback sets up masks, backdrop, collision, lights. See [Field: The map model and the map objects](game/field.md#the-map-model-and-the-map-objects).

**`mapSceneScriptBuffer`**: The 131072-byte buffer of the place script image and its library. It is also the key of all threads of the place. See [Events: Starting the place script](scripting/events.md#starting-the-place-script).

**Marks**: 21 bits per unit next to skills and ethers (`battleSlotMarkSet`). KOS-MOS starts with marks 1 to 3. See [Party and growth: Writing back](game/party-and-growth.md#writing-back).

**master_volume, master_level, voice_level, slot_level**: The four default levels in `soundCtrl`: overall volume (110), music (113), sound effects (100), ADX (100). See [Audio: The sound control block](architecture/audio.md#4-the-sound-control-block).

**Material batch**: Batch of 16 to 97 quadwords with the GS register rows (TEX0, CLAMP, ALPHA, TEX1), the shading parameters and an optional list of animation frames. See [Material batches](formats/models-and-textures.md#material-batches).

**Material flags**: The flag word of a setup batch, kept at VU1 address 0x3c4. Known bits: 0x1 unlit, 0x4 textured, 0x8 second layer textured, 0x40 and 0x80 extra registers, 0x100 morph, 0x400 exempt from fog and black fog, 0x800 clip triangles (Map), 0x4000 distance fade (Map). See [pxy: The setup batch and the material](libraries/pxy.md#the-setup-batch-and-the-material).

**Material key**: One of the 14 three-letter keys of `footMaterialKeys` (`kon`, `rai`, `gla`, `flo`, `iro`, `sto`, `car`, `woo`, `clo`, `wat`, `tur`, `ear`, `sno`, `sna`). See [Sound files: Footstep banks](formats/sound.md#footstep-banks).

**Material kind**: The number in lane x of a setup batch header. It selects the drawing routine for part style 0: 5 to 12 in the lit programs, 0 to 4, 8 and 13 in the Map program. See [pxy: Material kinds and part styles](libraries/pxy.md#material-kinds-and-part-styles).

**Material table**: Table of 0x60-byte records at 0x4f0 with the name, type, texture number, opacity and texture window of each material. The game code does not read it. See [The material table](formats/models-and-textures.md#the-material-table).

**math**: Module of 4 natives (`atan`, `sin`, `cos`, `random`) with results scaled by 10000. See [Native functions: The math module](scripting/natives.md#the-math-module).

**math natives**: The script module `math` with `atan`, `sin`, `cos`, `random`, using hundredths and results times 10000. See [Math: Script math natives](libraries/math.md#script-math-natives).

**Matrix (union)**: The `xgl_2.h` union that views one matrix as 16 floats, rows, words, quadwords or vectors. See [Math: Types](libraries/math.md#types).

**Matrix heap**: `RgHeap` 5, `0x38000` bytes in a static buffer, holds the joint matrix arrays of the actors. See [Memory: RgHeap](architecture/memory.md#rgheap).

**Matrix index**: The position of a node in the file order of the node groups of a hierarchy, counted from 0. Parents, collision records, attach points and the model refer to nodes by it. See [Skeletons and motion: Conventions shared by all formats](formats/skeletons-and-motion.md#conventions-shared-by-all-formats).

**Matrix stack**: The `xglMatrixStack` interface: a current matrix in `vf28`-`vf31` and saved matrices inside VU0, driven by the microprogram. See [Math: The matrix stack](libraries/math.md#the-matrix-stack).

**Matrix4**: A 4 x 4 float matrix of 64 bytes, stored as four rows. See [Math: Matrix layout](libraries/math.md#matrix-layout-rows).

**`mc_check()`**: The boot-time memory card check. Warns when no card is present or when no card has a save or 34 KB free. See [Boot and title: The memory card check](game/title-and-boot.md#4-the-memory-card-check).

**MC_REQ_CHECK_OLD (old-data check)**: Memory card command that searches the save folders of Xenosaga Episode I, Episode I Reloaded and Xenosaga Freaks and reports a few bytes of each save. See [Save data: Save data of the earlier games](architecture/save-data.md#save-data-of-the-earlier-games).

**MC_REQ_CREATE**: Request that makes the save folder, deletes the old save, writes the three icons and a filler file. See [Save data: Creating a save folder](architecture/save-data.md#creating-a-save-folder).

**McCreateData / carddata.bin**: The icon image handed to the create request: sixteen offsets, the `icon.sys` contents at `+0x40`, then the icon models. See [Save data: Creating a save folder](architecture/save-data.md#creating-a-save-folder).

**McListWork**: Work buffer of the list and old-data scans: 100 slot bytes, newest number, status, has-icon byte, directory table, file data at `+0x2000`. See [Save data: The list scan](architecture/save-data.md#the-list-scan).

**MCU**: Minimum coded unit of a JPEG: 8x8, 16x8 or 16x16 pixels depending on the sampling mode. See [Save data: The JPEG codec](architecture/save-data.md#the-jpeg-codec).

**MEf effect**: A battle effect written as code in OV01 (`src/ov01/m_ef*.c`) that lives in an MSe object, computes its own geometry and sends its own GS packets. See [Battle effects: The effect families](game/battle-effects.md#the-effect-families).

**Member choice**: The step on slot 7 where the player picks the character a page works on. See [Main menu: Focus functions](game/main-menu.md#focus-functions).

**Member id**: Small number that names a party member and is also its `bat.dat` unit number and parameter block key. 1 chaos, 2 KOS-MOS, 3 Shion, 4 Jin, 5 Ziggy, 6 MOMO, 7 Jr., 11 to 13 the three E.S. See [Party and growth: The cast and its ids](game/party-and-growth.md#the-cast-and-its-ids).

**memberMask**: One bit per member who has ever joined (bit `id - 1` for characters, `id - 4` for E.S.). See [Party and growth: The party state](game/party-and-growth.md#the-party-state).

**Memory card request**: An entry of the 16-entry queue of `xglMcRequest`, executed step by step by `xglMcMain`. See [xgl: Memory card](libraries/xgl.md#memory-card).

**Memory card status word**: Two 16-bit statuses (slot 1 low): 0 none, 1 ready, 2 unformatted, `0x80` error, `0x81` PS1 card, `0x82` PocketStation, `0xFF` busy. See [Save data: The state machine](architecture/save-data.md#the-state-machine).

**menu (module)**: Module of 11 natives that change the party (`ptJoin`, `ptSet`, `ptChangeMode`, ...). See [Native functions: The menu module](scripting/natives.md#the-menu-module).

**Menu arena**: The 0x288000-byte block from `GameResourceWorkAlloc` that holds everything the menu loads (`xglCdArenaOpen`). See [Main menu: Memory and files](game/main-menu.md#memory-and-files).

**Menu layout**: One of ten arrangements of the party panels in `menuLayoutSlotTable`. See [Main menu: MenuBlock, the party panels](game/main-menu.md#menublock-the-party-panels).

**menu owner**: The script native owner `menu` with eleven natives (`ptInit` to `umnReturn`). See [Main menu: Script natives](game/main-menu.md#script-natives).

**Menu text layer**: The `textDraw*` functions at the top of `src/main/menu_draw.c`. See [Fonts and text: The menu text layer](libraries/fonts-and-text.md#the-menu-text-layer).

**MenuBlock**: The cursor menu whose items are party members (`mainMenuBlock`, `itemMenuBlock`). See [Main menu: MenuBlock, the party panels](game/main-menu.md#menublock-the-party-panels).

**menuCopilotModeFlag**: The byte that holds the menu form, 1 in E.S. mode. See [Main menu: Foot mode and E.S. mode](game/main-menu.md#foot-mode-and-es-mode).

**MenuFileMain**: The save and load screen. Argument 0 saves, nonzero loads. Returns 0 only for a cancelled load. See [Save data: The save and load screen](architecture/save-data.md#the-save-and-load-screen).

**MenuItem**: One party panel of a `MenuBlock`: member index, position track, highlight track, flags. See [Main menu: MenuBlock, the party panels](game/main-menu.md#menublock-the-party-panels).

**menuLoadPending / menuRequestLoad**: Flag (initially 1) that makes the next `ptReset` do its one-time setup, and the function that sets it. See [Save data: A new game](architecture/save-data.md#a-new-game).

**menuSceneRunLoop**: The function that is the whole menu session: set-up, frame loop, tear-down. See [Main menu: One menu session step by step](game/main-menu.md#one-menu-session-step-by-step).

**menuSceneSetState**: The entry to a menu page by number (1 to 10). See [Main menu: Focus functions](game/main-menu.md#focus-functions).

**Mersenne Twister (MMathRandom)**: The MT19937 generator of the MMath library, used by battle effects and the effect noise table. See [Math: Mersenne Twister](libraries/math.md#mersenne-twister).

**Mesh entry**: 0x20-byte record (`PxyTableEntry`) with the offset and quadword count of a VIF stream, its material index and a bounding sphere. See [Mesh entries](formats/models-and-textures.md#mesh-entries). See also [Maps and field data: Mesh entries](formats/maps-and-field-data.md#mesh-entries).

**Message (SbMessage)**: A 16-byte record (kind, sender, two arguments) sent to a thread. See [The SB virtual machine: Messages](scripting/sb-vm.md#messages).

**Message page**: Page of one campaign: subject, location, client, portrait, status and message. See [G.S. Campaign: The message page](game/gs-campaign.md#the-message-page).

**Message queue (SbQueue)**: The ring of 16 messages of a thread. See [The SB virtual machine: Messages](scripting/sb-vm.md#messages).

**Message record**: A subtitle with first and end frame and an optional voice stream. See [Cutscenes: Messages, subtitles and voices](scripting/cutscenes.md#messages-subtitles-and-voices).

**Message slot (`MsgSlot`)**: A 0x98-byte window record with a common 0x28-byte header (flags, position, size, depth, open counter, type, frame colour) and a per-type body. See [UI windows: MsgSlot](libraries/ui-windows.md#msgslot).

**Message system**: The retained window toolkit of the field: dialogue text, script menus, the sell window, the item notice, number entry and the countdown clock, all built from message slots. See [UI windows: Three window families](libraries/ui-windows.md#three-window-families).

**MGsPacket**: The second packet builder of OV01 (`MGsGPInit`, `MGsGPTerm`), used by the `MEf` effects. See [The battle engine: Scene state and the MGs packet](game/battle-engine.md#scene-state-and-the-mgs-packet). See also [Battle effects: GS packets](game/battle-effects.md#gs-packets).

**Micro mode**: VU0 running a routine of its own program, started with `vcallms` or `vcallmsr`. See [Math: Micro mode](libraries/math.md#micro-mode-and-the-resident-microprogram).

**MIDI batch queue**: The queue in the KORG client that `korgEvagMidiProcBatchAdd` fills and `korgEvagMidiProcBatchFlush` sends once per frame. See [Audio: Voices](architecture/audio.md#8-voices-sound-effects-as-midi-messages).

**Minigame overlays**: OV10, OV11 and OV12 of Episode I, absent from Episode II. See [From Episode I to Episode II: 1. Programs and overlays](architecture/from-episode-i.md#1-programs-and-overlays).

**Missile trail**: Fire type 11: five missiles flying straight with ribbon trails. See [Battle effects: Missile trail (type 11)](game/battle-effects.md#missile-trail-type-11).

**MMath**: The VU0 macro-mode vector and matrix library of the battle and effect code. See [Math: MMath](libraries/math.md#mmath).

**MMathUnitMatrix / MMathUnitVector**: The identity matrix and the point (0, 0, 0, 1) that MMath substitutes for null arguments. See [Math: MMath](libraries/math.md#mmath).

**Mode**: The number of a body motion inside a motion table. See [Actors: Modes and slots](libraries/actors.md#modes-and-slots). See also [Animation: Selecting an entry](libraries/animation.md#selecting-an-entry). See also [Skeletons and motion: Purpose and location](formats/skeletons-and-motion.md#purpose-and-location-1).

**Mode hook**: A function that `MSeObjProc` calls after its mode 1 or mode 2 pass (`MSeSetMode1Hook`, `MSeSetMode2Hook`). Never installed in the game. See [Battle effects: The object pool](game/battle-effects.md#the-object-pool).

**Mode slot**: One of four motion tables an actor holds (`rsrc[4..7]`). Field enemies encode a motion as slot × 100 + mode. See [Actors: Modes and slots](libraries/actors.md#modes-and-slots).

**Model entry**: The entry point of a model program that `pxyDrawPartData` calls with `MSCAL`. It is a dispatcher that runs once per batch. See [pxy: Batches of a part](libraries/pxy.md#batches-of-a-part).

**Model entry, GS kick entry, wide GS kick entry**: The three `MSCAL` addresses of each model program. See [Rendering: VU1 programs](architecture/rendering.md#vu1-programs).

**Model flags**: The word at +0x08 of the model header that selects the special paths of the model library. See [Model flags](formats/models-and-textures.md#model-flags).

**Model key**: The resource id of a player model. `0x10000` to `0x13FFF` are characters, `0x14000` up are E.S. See [Field: Model keys](game/field.md#model-keys).

**Model packet (`PxyPacket`)**: The copy of the next-model block in the packet buffer, read by all draw code. See [pxy: The next-model block and the packet](libraries/pxy.md#the-next-model-block-and-the-packet).

**Model pass, screen sprite pass, world sprite pass**: The three effect draw passes of a frame, selected by `_draw3D` = 2, 1, 0. See [sef: The three passes](libraries/sef.md#the-three-passes).

**Model programs**: The five VU1 programs that draw `.pxy` models: Plain, Map, Break-up, Field and Battle. Only one is loaded at a time. See [pxy: The model programs](libraries/pxy.md#the-model-programs).

**Model window**: Screen rectangle that is both a 2D frame and the window of camera 1, in which the guide model is drawn. See [G.S. Campaign: Common drawing](game/gs-campaign.md#common-drawing).

**Monitor (`CfeMonitor`)**: An IPU movie drawn on a quad inside the 3-D scene. See [Cutscenes: Monitors](scripting/cutscenes.md#monitors).

**Monitor movie**: An IPU movie loaded by `cfe.ldMnt` and drawn on a rectangle in a cutscene. See [section 4.1](formats/movies-images-and-icons.md#41-where-they-are-and-who-plays-them).

**monsGainSet**: Adds one defeated enemy's reward record to the total and rolls its one drop. See [Party and growth: Collecting rewards](game/party-and-growth.md#collecting-rewards).

**Monster set (`groups[8]`)**: Enemy party number. One of the eight entries of an enemy is chosen at random on contact and read by the battle with `cfMonsSetNoGet`. See [Field enemies: The request](game/field-enemies.md#the-request).

**Monster set numbers**: The eight bytes of an enemyorder row, one of which is picked at random for the battle. See the same section.

**Monster slot**: A `MonsSlot` record with one rigid actor. The battle uses it for the model of the battle map, not for enemies. See [The battle engine: Monster slots](game/battle-engine.md#monster-slots).

**Morph record**: A quadword at 0x37c with weight, offset and stride, used to mix positions and normals with targets in the batch. See [pxy: A vertex batch](libraries/pxy.md#a-vertex-batch).

**Mosaic**: defocus type 5, pixel blocks made with the GS region repeat mode. See [Screen effects: The types](libraries/screen-effects.md#the-types).

**Motion clock**: The frame position of a motion state, advanced by `speed` times the step of studio camera 0 each frame. See [Animation: The motion clock](libraries/animation.md#the-motion-clock).

**Motion entry (`TiOffsetEntry`)**: One motion: version, first and last frame, and the counts and offsets of its curve streams (two layouts, split at version 1.4). See [Animation: Motion tables and entries](libraries/animation.md#motion-tables-and-entries).

**Motion entry (XAC)**: One motion of a pack (`TiOffsetEntry`), magic `XAC `, version 1.2 to 1.4, a frame range and one curve stream per node group. See [Skeletons and motion: Entries](formats/skeletons-and-motion.md#entries).

**Motion name table (`dataMotNameTbl`)**: 218 names of `yamamoto\mot\*.xap`, indexed by unit key up to 140 and by the technique field `timer` from 168. See [Motion files](formats/game-tables.md#motion-files-in-yamamotomot).

**Motion pack (XAP)**: A `.xap` file, magic `XAP `, a numbered list of motion entries for one skeleton (`OffsetTable`). See [Skeletons and motion: Motion packs (XAP)](formats/skeletons-and-motion.md#motion-packs-xap).

**Motion package (`motionPack`)**: The `TiMRPackage` embedded at `0x640` of the actor, which binds hierarchy, motion and matrices. See [Actors: The motion package inside the record](libraries/actors.md#the-motion-package-inside-the-record).

**Motion state (`TiMRState`)**: One clock of the package (0x50 bytes): flags, speed, frame position, blend length, play range. State A is the body, state B the face. See [Animation: The motion state](libraries/animation.md#the-motion-state).

**Motion table (`.xap`, `OffsetTable`)**: A counted table of motion entries addressed by byte offsets. See [Animation: Motion tables and entries](libraries/animation.md#motion-tables-and-entries).

**Motion/render package (`TiMRPackage`)**: The 0x100-byte working state of one animated model inside an actor (offset 0x640): data pointers, matrix buffers and two motion states. See [Animation: The motion/render package](libraries/animation.md#the-motionrender-package).

**Move dispatcher**: Commands 16 and 17, which choose a run, jump or hover mover from `stats->subState` and replace their own command number. See [The battle engine: Moving](game/battle-engine.md#moving).

**Move mode (camera)**: The protocol `MoveRefMode` / `MoveCamMode`, a `Set` native, then `Move`, which turns a cut into an interpolated move. See [Native functions: Moves](scripting/natives.md#moves).

**Move slot**: One of four interpolation slots of the camera: 0 reference point, 1 camera position, 2 perspective, 3 bank. See [Native functions: The camera module](scripting/natives.md#the-camera-module).

**Movie key**: The text `%08x.%08x` (first sector, size in sectors) that `moviePlayerStartFile` hands to the CRI player instead of a path. See [section 2.7](formats/movies-images-and-icons.md#27-how-the-player-reads-the-stream).

**`MoviePlayerRecord`**: The game's record of one Sofdec player. See [Cutscenes: The player wrapper](scripting/cutscenes.md#the-player-wrapper).

**Moving window**: The per-sample flag words (active, START, END) that are shifted one sample per frame so that a lit stretch travels along a fixed curve. See [Battle effects: The moving window](game/battle-effects.md#the-moving-window).

**MSe object**: A 0x800-byte visual effect object of the 127-slot pool at `0x01FB0000`. Not a sound object. See [Audio: The MSe object pool](architecture/audio.md#21-the-mse-object-pool). See also [Battle effects: The object pool](game/battle-effects.md#the-object-pool).

**MSe pool**: The fixed block at 0x01FB0000 that holds the MSe objects, managed by `MSeObjInit`, `MSeObjAlloc`, `MSeObjRequestFree` and `MSeObjProc`. See [Battle effects: The object pool](game/battle-effects.md#the-object-pool).

**MSeFire**: The dispatcher that creates `MSe` effect objects of one of 14 types through `MSeFireCallbacks`. See [The battle engine: Scene state and the MGs packet](game/battle-engine.md#scene-state-and-the-mgs-packet). See also [Battle effects: MSeFire and the fire entry table](game/battle-effects.md#msefire-and-the-fire-entry-table).

**MSeFireEntry**: The 0x40-byte request that `MSeFire` copies into a new battle effect object. See [sdv: The effect sequence](libraries/sdv.md#the-effect-sequence).

**MSeObjProc mode**: One of the four passes over the pool: 0 update, 1 model draw, 2 packet draw, 3 advance and free. Modes 0 and 1 run in `sefDrawEffect`, 2 and 3 in `sefDrawEffect2D`, in battle mode only. See [Battle effects: Object lifecycle and frame timing](game/battle-effects.md#object-lifecycle-and-frame-timing).

**`msgBalance`**: The remaining debt, saved as `SaveData.balance`. See [G.S. Campaign: The debt](game/gs-campaign.md#the-debt).

**`msgChoice`**: A free-standing message slot that is either the "Acquired" item notice (type 4) or the number entry box (type 3). See [UI windows: The message objects](libraries/ui-windows.md#the-message-objects).

**`msgPanel`**: A free-standing message slot that shows the countdown clock of timed sections. See [UI windows: The countdown panel](libraries/ui-windows.md#the-countdown-panel).

**MsgRange**: A typed-out message: text, token count, reveal position. Used for the help line. See [Main menu: The typed-out help line](game/main-menu.md#the-typed-out-help-line).

**MsgRange (help line)**: The revealed help text at the bottom of menu screens, with `&NN` icons. See [Fonts and text: The help line and its icons](libraries/fonts-and-text.md#the-help-line-and-its-icons).

**`MsgWindow`**: A 0x308-byte stack of five message slots plus `current_index`, the topmost open slot. The game has one, `msgWindow`. See [UI windows: MsgWindow](libraries/ui-windows.md#msgwindow).

**`msgWindowFlush`**: Runs the pending window and the queued slots (update, then draw), then clears the queue and the hook. See [UI windows: One frame of the message system](libraries/ui-windows.md#one-frame-of-the-message-system).

## N

**Name pool**: `resNamePool`, 1024 bytes of text for effect names and sound records of the entry table. See [Events: The entry table](scripting/events.md#the-entry-table).

**Name record**: A compressed name in the archive directory. A count byte of 0 gives a full name, any other count overwrites a few characters near the end of the previous name (`xglCdArcNameExpand`). See [Files and resources: The archive directory](architecture/files-and-resources.md#the-archive-directory).

**Name stem**: The six-character family name of an enemy (`gno001`, `uro001`, `bos018`) shared by its motion file, its model files and its script entries. See [Unit records](formats/game-tables.md#unit-records).

**Name table**: The table of an image that maps names to code positions (`SbNameEntry`). See [The SB virtual machine: Sections](scripting/sb-vm.md#sections). See also [Name table](formats/scripts-and-archives.md#name-table).

**Named block**: A block loaded with `GameResLoadName`. It starts with a 64-byte header holding the file name. See [Memory: Loading into blocks](architecture/memory.md#loading-into-blocks).

**Native**: A C function that scripts call by owner name and method name. See [The SB virtual machine: Natives](scripting/sb-vm.md#natives).

**Native function**: A C function of the game that an SB script calls by a module name and a function name, for example `objx.move`. See [Native functions](scripting/natives.md#native-functions).

**Native id**: `(owner index << 16) + method index`, the operand of opcode `0x15` after linking. Valid only for the registration order it was linked against. See [The SB virtual machine: Linking](scripting/sb-vm.md#linking). See also [Native functions: The registry](scripting/natives.md#the-registry).

**Native registry**: The arrays `sbNativeOwners` (0x600) and `sbNativeMethods` (0x800) in the overlay. See [The SB virtual machine: The registry](scripting/sb-vm.md#the-registry). See also [Native functions: The registry](scripting/natives.md#the-registry).

**Neighbour link**: The index of the triangle across an edge, -1 for an open edge. See the same section.

**New Game**: `TitleGameInit`, fade, `configScreenRun`, then `nextMap = 7001`, `loadStage = 0`, `Game()`. See [Boot and title: New Game](game/title-and-boot.md#new-game).

**New mark (`YG2PATH_FLAG_NEW`)**: Bit 0x10 that `questFlagSet` adds to status 2 or 3 and that opening the detail page clears. Hidden from scripts by `questFlagGet`. See [G.S. Campaign: Quest flags](game/gs-campaign.md#quest-flags).

**Next-model block**: The 0x2a0 bytes of globals from `nmlRenderStatus` that the setters fill and `pxySubmitModel` copies. See [pxy: The next-model block and the packet](libraries/pxy.md#the-next-model-block-and-the-packet).

**Nine-slice panel**: A box built from nine sprite cells with `panelDrawNineSlice`. See [Main menu: The text and sprite layer](game/main-menu.md#the-text-and-sprite-layer).

**nml names inside pxy**: the 42 `nml*` function names that exist in both games. See [From Episode I to Episode II: 6. Models and rendering](architecture/from-episode-i.md#6-models-and-rendering).

**Node**: A numbered point (0 to 31) on a skeleton: a bone matrix index, an offset and a rotation. See the same section.

**Node block**: The data of one node in a stream: a `TiXacTransformHeader` with `channelFlags` and `headerFlags`, then one curve per set channel bit. See [Skeletons and motion: Node blocks](formats/skeletons-and-motion.md#node-blocks).

**Node groups**: The seven groups of a hierarchy in matrix order: 2 locators, joints, second joints, third joints, skip (face) nodes, dynamics nodes, static nodes. See [Animation: Node groups](libraries/animation.md#node-groups).

**Node table (`TiNodeTable`, "look link")**: A list of attach points on a skeleton, indexed (version below 1.1) or searched by id. See [Animation: Node tables and attach points](libraries/animation.md#node-tables-and-attach-points).

**Noise table**: 1027 fixed random values between -0.5 and 0.5 used for effect randomness. See [sef: Random numbers](libraries/sef.md#random-numbers).

**North angle**: `CameraRange.northAngle`, the yaw that stick directions are measured from. See [Field: Moving](game/field.md#moving).

**Note map**: The 128 16-bit key indices of a program, one for each MIDI note, `0xFFFF` for a silent note. See [Sound files: Programs](formats/sound.md#programs).

**Notice (countdown)**: The timed message box of the menu (`countdownInit`, `countdownTick`). See [Main menu: Prompt and notice](game/main-menu.md#prompt-and-notice).

**`noticeCooldown`**: 60 frames after `Homing_Search` during which a character neither sees nor hears. See [Field enemies: The way home](game/field-enemies.md#the-way-home).

**Number entry box**: The editable timecode slot in mode 0 opened by `talk.slotWinOpen`: LEFT and RIGHT select a digit, UP and DOWN change it without carry, CIRCLE confirms. See [UI windows: The number entry box](libraries/ui-windows.md#the-number-entry-box).

**Nyuru**: The slide of an actor along a unit (`HitCheckMapUnitWithNyuru`, `NyuruMatrix`, `NyuruCircle`). A Japanese sound word for something slippery. See [Maps and units: Collision with units](game/map-and-units.md#collision-with-units).

## O

**Object slot table**: `objSlotTable`, 64 slots of `0xB0` bytes claimed with `objSlotAlloc`. It holds all HUD pieces, damage numbers and the arrow. See [The battle engine: The object model](game/battle-engine.md#the-object-model).

**objHatoVec**: The mover that sets the velocity to the remaining distance divided by a frame count and snaps axes closer than 0.5. Called every frame it gives the easing of the HUD. See [The battle engine: The slide block](game/battle-engine.md#the-slide-block).

**objStdMove**: The mover that advances a slide block by velocity and acceleration and reports arrival on all three axes. See [The battle engine: The slide block](game/battle-engine.md#the-slide-block).

**objx**: Module of 15 natives that move, rotate, scale and link map objects. See [Native functions: The objx module](scripting/natives.md#the-objx-module).

**Offset-table archive**: File that starts with a table of 32-bit words (table size, then item ends from the data base) and holds items on 64-byte boundaries. See [section 6.1](formats/movies-images-and-icons.md#61-the-common-layout).

**Old layouts (record table)**: Two older record table forms, in 68 and 15 scripts. See [Effect data files: two older record table layouts](formats/effects.md#two-older-record-table-layouts).

**oldDataMaximaCopy**: Hands the six maxima collected from old saves and the notice bits to `ptStateInit`, which awards skill points, an ether entry and an item. See [Save data: What the player gets](architecture/save-data.md#what-the-player-gets).

**oLib**: Helper file with a heap setup, object actors and degree tables. Mostly unused. See [xgl: oLib](libraries/xgl.md#olib).

**Omitted argument**: An argument slot with type byte 0, by which a script says "keep the current value" or "use the default". See [Native functions: Omitted arguments](scripting/natives.md#omitted-arguments).

**One-shot sample**: A sample whose blocks end with flag `0x01` and the end block `00 07 77 ...`. It plays once and the voice stops. See [Sound files: Sample data](formats/sound.md#sample-data).

**`ONE_OVERLAY`, `TWO_OVERLAY`**: `0x01000000` and `0x02000000`, added to a function address to build a selection. See [Boot and title: The title loop](game/title-and-boot.md#3-the-title-loop-and-the-title-return-value).

**Opacity (font object)**: Second alpha factor of the font object layer (default 0x80), used for window fades and read back by the menu text layer. See [Fonts and text: Colour, opacity and depth](libraries/fonts-and-text.md#colour-opacity-and-depth).

**Opcode**: The first unit of an instruction, 0x00 to 0x2D. See [The SB virtual machine: Instruction set](scripting/sb-vm.md#instruction-set).

**Open counter**: `MsgSlot.open_counter`, 0 to 12, the frame counter of the opening and closing animation of every slot type. See [UI windows: Opening and closing](libraries/ui-windows.md#opening-and-closing).

**Open edge**: A triangle edge without a neighbour. It stops or deflects a move. See the same section.

**Option word (`msgSlotFlags`)**: One word per open message set by the script (`talk.msgDtl` argument 1): no CIRCLE, no page cursor, keep the game flag, CROSS closes, and two bits for the map object. See [UI windows: Input on an open message](libraries/ui-windows.md#input-on-an-open-message).

**Options page**: The settings page (`menuDrawConfig`, title "Configuration"). See [Main menu: Options](game/main-menu.md#options).

**Orbit placement**: Camera key mode 1: pitch, yaw and distance around a part instead of an offset. See [sdv: Position and look-at keys](libraries/sdv.md#position-and-look-at-keys).

**Order key**: The third argument of the print calls (named `color` in the source). Bits 24-27 pick the bucket and the low 24 bits are the GS depth. See [Fonts and text: The order key](libraries/fonts-and-text.md#the-order-key).

**Order number**: The `key` of an enemyorder row (101 to 1301, grouped by hundreds) that a place script passes to create an enemy. See the same section.

**OV01 heap**: 10 MiB first-fit heap with `0x40`-byte block headers in `src/ov01/data_unit_org_get.c`. See [Party and growth: The overlay heap and file helpers](game/party-and-growth.md#the-overlay-heap-and-file-helpers).

**ov01DrawAll**: The draw pass of the battle engine. It is installed as `sRender.battle_draw_callback`, masks depth writes and sweeps the draw callbacks. See [The battle engine: One frame of the engine](game/battle-engine.md#one-frame-of-the-engine).

**ov01StepAwaitLoads**: The wait step that the loader pushes on the step stack. It pops itself when no disc read and no effect load is pending. See [The battle engine: The step stack](game/battle-engine.md#the-step-stack).

**ov01UpdateAll**: The update pass of the battle engine, called once per frame by `ov01SceneLoop`. It sweeps the callback slots, tasks, monster slots, units, data objects and object slots in that order. See [The battle engine: One frame of the engine](game/battle-engine.md#one-frame-of-the-engine).

**OV02 (title overlay)**: The small overlay with the boot check, logos, title screen and HDD dialogs. A copy is embedded in the main executable, so it is resident at boot. See [Boot and title: The two overlays and the two discs](game/title-and-boot.md#2-the-two-overlays-and-the-two-discs).

**Overlay**: `OV01.OVL` or `OV02.OVL`, an ELF loaded at `0xA80000` by `xglCdLoadOverlay`. Only one is in memory at a time. See [Files and resources: Overlays and IOP modules](architecture/files-and-resources.md#overlays-and-iop-modules).

**Overlay batch**: Batch of a model with header flags 0x81, drawn before the sorted models and used as shadow receiver. In practice the map. See [pxy: The batch lists](libraries/pxy.md#the-batch-lists).

**Overlay slot**: The 512 KiB at `0x00A80000` that holds OV01 or OV02, cleared and reloaded by `xglCdLoadOverlay`. See [Memory: The EE memory map](architecture/memory.md#the-ee-memory-map). See also [Boot and title: The two overlays and the two discs](game/title-and-boot.md#2-the-two-overlays-and-the-two-discs). See also [From Episode I to Episode II: 1. Programs and overlays](architecture/from-episode-i.md#1-programs-and-overlays).

**Override matrix**: A pointer to a parent joint matrix that a joint-attached child multiplies into its matrix. See [Actors: Parents, children and attachments](libraries/actors.md#parents-children-and-attachments).

**Owner**: The word that says who holds a block: 0 (free), a file id, `0xF00000` plus address bits (anonymous or loaded by name), optionally with the preloaded flag. See [Files and resources: The block table](architecture/files-and-resources.md#the-block-table). See also [The SB virtual machine: The registry](scripting/sb-vm.md#the-registry).

**Owner (module)**: The name under which one table of natives is registered, such as `objx`, `camera` or `bat`. See [Native functions: Overview](scripting/natives.md#overview).

**Owner (of a block)**: 0 for a free block, a file id (low 24 bits of a resource id) for a loaded resource, or an anonymous tag `0xF00000` + address bits. See [Memory: Allocating and freeing blocks](architecture/memory.md#allocating-and-freeing-blocks).

**Owner table**: `resOwnerTable`, the owners of the resource blocks the next place does not need. `resApplyCfFile` frees them. See [Events: The entry table](scripting/events.md#the-entry-table).

## P

**Pack**: One 2,048-byte unit of a Sofdec file, starting with `00 00 01 BA`, a clock and the multiplex rate. See [section 2.2](formats/movies-images-and-icons.md#22-the-program-stream).

**Pack class**: The unread u16 at chunk offset 0x12, 1 in most chunks of the 123 ordinary packs, 0 elsewhere. See [Effect data files: the pack class field](formats/effects.md#the-pack-class-field).

**Packet**: The list of DMA tags, VIFcodes and data that describes one frame. Two packets alternate, one being written and one being drawn. See [Rendering: Packets](architecture/rendering.md#packets). See also [xgl: Packets](libraries/xgl.md#packets).

**Packet buffer**: One of the two 2 MiB halves at `0x00C00000` and `0x00E00000`. One is filled while the other is sent. See [Memory: Packet buffers as frame memory](architecture/memory.md#packet-buffers-as-frame-memory).

**Packet limit**: `XglPacket.limit`, the line between DMA chain and attribute data. `xglPacketMove` sets the cursor back to it. See [Memory: Packet buffers as frame memory](architecture/memory.md#packet-buffers-as-frame-memory).

**Packet mode**: `xglPacketMode`, 0 to 4, the VU1 program currently loaded for models. See [Rendering: VU1 programs](architecture/rendering.md#vu1-programs). See also [pxy: The microcode programs](libraries/pxy.md#the-microcode-programs).

**Pad override**: A synthetic controller state for the camera travel modes, valid for one frame. See [xgl: Travel modes](libraries/xgl.md#travel-modes).

**Pad setup state**: `setupState`, the per-port state machine from probe to ready. See [xgl: The setup state machine](libraries/xgl.md#the-setup-state-machine).

**padApplySaveSettings**: Applies the saved vibration switch, sound mode and screen position. First statement of `Game()`. See [Save data: The header and the options](architecture/save-data.md#the-header-and-the-options).

**PadData / PadRecord**: The two controller records (`0x80` bytes each) that `xglPadRead` fills once per frame. See [xgl: The pad record](libraries/xgl.md#the-pad-record).

**Padding pattern**: The 32-byte text `MONOLITHSOFT Xenosaga Episode.2\0` that fills every unused sector tail, with a phase equal to the byte offset in the group modulo 32. See [Padding](formats/scripts-and-archives.md#padding).

**Page buffer**: The 0x140000-byte part of the arena that is emptied with `xglCdBufferRewind()` when a page needs it. See [Main menu: Memory and files](game/main-menu.md#memory-and-files).

**Page cursor**: The bobbing mark after the last shown character of a text slot that waits for CIRCLE. See [UI windows: Drawing the text](libraries/ui-windows.md#drawing-the-text).

**Page marker**: The bytes `A1 FC` (●) inside a battle text entry. Marks a reveal step and is never drawn. See [Fonts and text: Page markers](libraries/fonts-and-text.md#page-markers). See also [Text and fonts: Page markers](formats/text-and-fonts.md#page-markers).

**Page selector**: the number 0, 1 or 2 (display, draw, work page) that `gsMakeFrame`, `gsMakeTex0` and `GetTex0` turn into a GS register value. See [Screen effects: Pages and coordinates](libraries/screen-effects.md#pages-and-coordinates).

**Pair**: One 8-byte VU instruction word: an upper instruction for the vector units and a lower one for loads, branches and integers, run in the same cycle. VU1 holds 2048 pairs. See [pxy: VU1 microcode](libraries/pxy.md#vu1-microcode).

**Panel row**: One of ten screen rows (y = 100 to 280) that left-side HUD panels take with `menuPanelRowAlloc`. See [The battle engine: Panel rows](game/battle-engine.md#panel-rows).

**Parent and attached unit**: A unit that follows another (`parent`) or is drawn in its coordinate system (`attached` 1). See [Maps and units: Tweens and the objx natives](game/map-and-units.md#tweens-and-the-objx-natives).

**Part**: A joint or attachment point of an actor that a select or point kind names. See [sdv: Actor select bits](libraries/sdv.md#actor-select-bits). See also [Parts](formats/models-and-textures.md#parts).

**Part (`PxyPart`)**: The unit of drawing, a 0x60-byte record with one layer, one matrix index, flags and a bounding sphere. See [pxy: Parts](libraries/pxy.md#parts).

**Part actor**: A kind 5 unit hung on a joint of the player actor (weapons on foot). See [Maps and units: The player record and its forms](game/map-and-units.md#the-player-record-and-its-forms).

**Part data record (`PxyTableEntry`)**: Offset and quadword count of the VIF data of a part inside the file. See [pxy: Part data and materials](libraries/pxy.md#part-data-and-materials).

**Part matrices**: One matrix per geometry that places the pieces of a rigid model. See [pxy: Bones and matrices](libraries/pxy.md#bones-and-matrices).

**Part range**: One of two 0x20-byte records at 0x4b0, named `non-alpha` and `alpha`, that split the parts into opaque and translucent. See [Part ranges](formats/models-and-textures.md#part-ranges).

**Part range (`PxyPartRange`)**: A run of parts. Range 0 is the opaque parts, range 1 the translucent parts. See [pxy: Parts](libraries/pxy.md#parts).

**Part style**: Small number sent to VU1 at 0x3c6 that selects normal drawing, Z-write, shadow texture, shadow receiver, planar shadow or screen sampling. See [pxy: Per-model and per-part data](libraries/pxy.md#per-model-and-per-part-data).

**Part visibility record (`CfeFlagRecord`)**: Value plus track per model geometry, set by `cfe.stKyAtPrts`. See [Cutscenes: Loading](scripting/cutscenes.md#loading).

**Particle pool (`sefEntryPool`)**: 2048 shared particles of 0xA0 bytes for the built-in emitters. See [sdv: Slots, emitters and the particle pool](libraries/sdv.md#slots-emitters-and-the-particle-pool).

**Particle slot ring**: Ring of 1,100 free indices (`sefPtFreeSlots`) over the particle table `_ptAlloc` (`sevAllocPtAllocator`, `sevFreePtAllocator`). See [Memory: The particle slot ring](architecture/memory.md#the-particle-slot-ring).

**Parts animation (`TiPamData`)**: Animation of rigid model parts without a skeleton: nine curves per part (offset, rotation, pivot). See [Animation: Parts animation](libraries/animation.md#parts-animation).

**Party snapshot**: The copy of each party unit made by `partySnapshotBuild` when the scene is released. The result screen works on it. See [Battle: The scene loop](game/battle.md#the-scene-loop).

**Party snapshots**: Copies of the up to three party battle units made by `partySnapshotBuild` when a battle ends, read by the result code. See [Party and growth: Paying out](game/party-and-growth.md#paying-out).

**PartyMember (member record)**: One of ten 16-byte records `ptMemberRecords`: the id and pointers to the status record, the point record and the skill board record. See [Party and growth: The member record](game/party-and-growth.md#the-member-record).

**PartyMemberStatus**: Name text, second text and icon index of a member (`ptMemberStatuses`). See [Party and growth: The member record](game/party-and-growth.md#the-member-record).

**PartyProgress (ptProgressRecord)**: Member mask, play time base, attach map and position, and the special item bitmap. See [Party and growth: The party state](game/party-and-growth.md#the-party-state).

**PartyTakenBits**: A `0x2B`-byte record per member with the skills taken on the skill board, copied whole into the save. See [Save data: The party block](architecture/save-data.md#the-party-block). See also [Main menu: Skills](game/main-menu.md#skills).

**PartyTakenBits (skill board record)**: `0x2B` bytes per member: opened classes, taken skills, highest open level and the four equip slots. See [Party and growth: The skill board record](game/party-and-growth.md#the-skill-board-record).

**PartyTimeDispChange**: Folds days into hours for display and caps the play time at 99:59:59. See [Save data: Dates and play time](architecture/save-data.md#dates-and-play-time).

**Path**: One of the 36 campaigns as the screen numbers them ("Path No. 01"). Same thing as a quest in `questFlags`. See [G.S. Campaign: The path grid](game/gs-campaign.md#the-path-grid).

**Path curve (`mefPathCurve`)**: The shared 0x2000-byte work area in which constructors build a Hermite curve (`MEfPathCurve`) before sampling it into their object. See [Battle effects: The path curve](game/battle-effects.md#the-path-curve).

**Path end marker**: A turning four-triangle star drawn by `MEfDrawPathEndMarker`, used by Thunder31 at its impact point. See [Battle effects: Marker drawers](game/battle-effects.md#marker-drawers).

**Path grid**: The honeycomb of 36 cells (6 columns, pitch 44, odd rows shifted 22) that shows the status of every campaign. See [G.S. Campaign: The path grid](game/gs-campaign.md#the-path-grid).

**Path step**: Fire type 9: the large-scale form of the Hermite path. See [Battle effects: Path step (type 9)](game/battle-effects.md#path-step-type-9).

**PATH1, PATH2, PATH3**: The three ways data reaches the GIF: kicked by VU1, passed by VIF1 in direct mode, or sent by the GIF's own DMA channel. See [Rendering: The hardware path](architecture/rendering.md#the-hardware-path).

**Pause cycle and threat cycle**: Two counters (`pauseCycle`, `threatCycle`) that make a walking character pause or pose once per cycle at a random step. See [Field enemies: Pause, threat and walking](game/field-enemies.md#pause-threat-and-walking).

**Pause draw hook**: `pauseDrawHook[0]`, a one-shot hook that draws the pause backdrop and title. See [Field: Pause](game/field.md#pause).

**Pending reads**: `xglCdPendingReads`, the count of asynchronous reads into arena memory. `xglCdWaitIdle` waits for it to reach zero. See [Memory: Pending reads](architecture/memory.md#pending-reads).

**Persistent slot**: A resource slot (kinds 0 to 4, 20, 21) that is kept when it has no particles and is saved by `sefCollectPersistentSlots`. See [sdv: Slots, emitters and the particle pool](libraries/sdv.md#slots-emitters-and-the-particle-pool).

**pFileTemp**: The 0x60000-byte text buffer of `cfeFileControl.c`. Same section.

**Phase (unit)**: The motion a unit plays. `unitSetPhase(unit, phase)` selects motion `phase - 1` of the unit's motion file. See [The battle engine: Phases are motions](game/battle-engine.md#phases-are-motions).

**Picture code**: `&NN` in menu help lines and battle messages. See [Text and fonts: Picture codes](formats/text-and-fonts.md#picture-codes) and [Button pictures](formats/text-and-fonts.md#button-pictures).

**Picture map**: A map whose texture data is a JPEG. It has no model and is drawn as a scrolling backdrop by `mapBackdropDraw`. See [Maps and units: Drawing the map](game/map-and-units.md#drawing-the-map).

**Picture pack**: Small archive layout of `carddata.bin`, `credit.bin`, `synopsis.bin`, `im.bin`, `titlebg2.bin`: word `4 × n`, n end offsets, items aligned to 64 from a 64-aligned base. See [The picture pack layout](formats/game-tables.md#the-picture-pack-layout).

**Picture pack header**: The shared layout of `credit.bin`, `synopsis.bin`, `im.bin` and `titlebg2.bin`: word 0 is the table size, further words are offsets from the 64-byte aligned end of the table. See [Boot and title: Data files](game/title-and-boot.md#data-files).

**Piece**: One data file of an archive group, exactly 1 GiB except the last. The reader never opens it by name. See [Groups and pieces](formats/scripts-and-archives.md#groups-and-pieces).

**Pitch**: A rotation about X. Positive pitch points the Z axis downward. See [Math: Axes, headings and pitch](libraries/math.md#axes-headings-and-pitch).

**Pitch (SPU2 pitch)**: `rate x 4096 / 48000`, the value of the SPU2 pitch register at which a sample plays at its recorded speed. Stored in every sample record. See [Sound files: Sample records](formats/sound.md#sample-records).

**Place (effect position)**: A numbered standing point of a battle, read with `effectPosGet` as three integers in hundredths of a unit. `stats->posIndex` is the place of a unit. See [The battle engine: Pose, angle and position](game/battle-engine.md#pose-angle-and-position).

**Place base**: The place number of room 01 of a zone. Room nn is place `base + 10 * (nn - 1)` for places below 6300. See [Maps and field data: Place numbers and maps](formats/maps-and-field-data.md#place-numbers-and-maps).

**Place number**: The four-digit decimal number of a place of the story. It names the scene resource script, the place script and the language library. Held in `gameWork.mapId` (current) and `gameWork.nextMap` (requested). See [Events: Place numbers](scripting/events.md#place-numbers).

**Place script**: The SB image `cf\scNNNN.sb`, read to `mapSceneScriptBuffer` and started by `mapSceneScriptLoad`. See [Events: Starting the place script](scripting/events.md#starting-the-place-script). See also [The place script](formats/scripts-and-archives.md#the-place-script).

**Placeholder record**: An unused enemy record whose name is its stem in full-width letters, with dummy numbers. See [Unit records](formats/game-tables.md#unit-records).

**Placeholder sample**: A sample named `-- no sample --` with 64 bytes of non-audio data. Sample 0 of every bank is one, and placeholders fill unused note numbers. See [Sound files: Placeholder samples](formats/sound.md#placeholder-samples).

**Placement table**: Group `0x300`: rows of `UwamonoEntry` (x, y, z, angle in 4096ths of a turn, key) used by ladders and locators. See [Maps and units: Blocks, groups and entries](game/map-and-units.md#blocks-groups-and-entries).

**Plain program**: `xglPlainMicrocodeProgram`, mode 0, the default lit program with six directional lights, two ambient colors and point lights. See [pxy: The model programs](libraries/pxy.md#the-model-programs).

**Planar shadow**: The model drawn flattened on a ground plane with `pxyShadowMatrix`. See [pxy: Planar shadows](libraries/pxy.md#planar-shadows).

**Plane (of a mesh)**: Four floats of which x and z are the slopes used by `mapPlaneDistance`. See [Maps and field data: Vertices and planes](formats/maps-and-field-data.md#vertices-and-planes).

**Plane set**: The camera's pair of matrices (`volume[0].planeSet[0]`) that hold three clip planes each as columns, used for sphere culling. See [Math: Sphere tests](libraries/math.md#sphere-tests).

**Play range**: A sub-range of a motion set with `XenoActorSetPlayRange`. See [Actors: Play range, speed and face animation](libraries/actors.md#play-range-speed-and-face-animation).

**Play time**: Stored as an `XglClock` at image `0x68`. Kept live as a reference clock reading in `ptState.progress->playTime`, so no frame counting is needed. See [Save data: Dates and play time](architecture/save-data.md#dates-and-play-time).

**Player part**: A record of kind 5 (index in `uwamonoPartIndices`) whose model hangs on a joint of the player. See [Field: Creating and changing the player](game/field.md#creating-and-changing-the-player).

**Player record**: The map object record of kind 4 at `uwamonoCurrentIndex`, which owns the player's actor. See [Field: The player](game/field.md#the-player). See also [Maps and units: The player record and its forms](game/map-and-units.md#the-player-record-and-its-forms).

**Player slot**: One of three permanent blocks of `0xC5FC0` bytes (plus a `0x40`-byte pad) for the file sets of the party members in the field. See [Files and resources: Player slots](architecture/files-and-resources.md#player-slots).

**`pLocalMat` / `pGlobalMat` / `pBlendMat` / `pPartsMat` / `pDynamics`**: The five heap buffers of an actor: local, global and blend-start matrices per hierarchy transform, part matrices per model geometry, dynamics bodies per dynamics node. See [Actors: The life of an actor](libraries/actors.md#the-life-of-an-actor).

**Point**: One particle of 0x2C0 bytes, a sprite or a small model. 1100 exist. See [sef: Points](libraries/sef.md#points).

**Point allocator**: Ring of free point handles (`sefPtFreeSlots`) with head and tail in the prefix of point entry 0. See [sef: The point allocator](libraries/sef.md#the-point-allocator-sev).

**Point record (PartyMemberExpPoints / SlotRecord)**: `0x40`-byte record of `slotRecordTable` per unit: total experience, experience countdown, Skill Points, Class Points, growth values and the learned bits. See [Party and growth: The point record](game/party-and-growth.md#the-point-record).

**Point table**: Twelve words of a line: point life, speed variation and the offsets of the scale, angle, colour, alpha, part, spin and speed tracks and of the image list. See [Effect data files: the point table](formats/effects.md#the-point-table).

**Point track**: One of the seven keyed tracks each point of a line reads: scale, angle, colour, alpha, spin, part, size. See [sef: Lines](libraries/sef.md#lines).

**Pop-up number**: The damage number object of `objDmgNumCreate`: up to six digits that unfold over a few frames and vanish after 25, coloured by hit kind. See [Battle effects: The pop-up number](game/battle-effects.md#the-pop-up-number).

**Portrait index table**: `yg2PathPortraitIndexTable`, which maps each of the 36 paths to the citizen who is its client. See [G.S. Campaign: Portraits](game/gs-campaign.md#portraits).

**Pose (unit)**: The slide block at `0x150` of a unit, the master copy of its position and facing. `objPosePublish` copies it to the actor. See [The battle engine: Pose, angle and position](game/battle-engine.md#pose-angle-and-position).

**Position kind**: Number that says relative to what a position is meant: world, camera, parent point, group centre, an actor or one of its attach points. See [sef: Position kinds](libraries/sef.md#position-kinds).

**Position table**: The scene's table of 16-byte position records (hundredths of a unit) in `effect_pos.c`, used by `createEffectPos` and unit placement. See [Battle effects: Other senders](game/battle-effects.md#other-senders). See also [Enemy groups](formats/game-tables.md#enemy-groups).

**Preload**: Loading the files of a neighbour place one entry per frame while the player is still in the current place (`resLoadCfFile`, `resPreloadStep`). Blocks carry `RES_OWNER_PRELOADED` (`0x40000000`). See [Events: Loading in the background](scripting/events.md#loading-in-the-background).

**Preloading**: Loading the resources of the next scene in the background, one entry per frame, while the player is near an exit. Preloaded blocks carry `RES_OWNER_PRELOADED` (bit 30). See [Files and resources: Preloading](architecture/files-and-resources.md#preloading).

**Prepare-then-play**: Pausing an ADX slot, starting a file, waiting for status 3 and releasing the pause on an exact frame (`adxLoad`/`adxPlay`, cutscene audio). See [Audio: ADX streams](architecture/audio.md#11-adx-streams).

**presentMask**: One bit per member of the active group that is in a slot. See [Party and growth: The status block](game/party-and-growth.md#the-status-block).

**Preset**: One record of an `.ene` file, chosen by the third argument of `enepc.setParams`. See [Loading](formats/scripts-and-archives.md#loading).

**Preset (`EnepcParamSet`)**: One record of an `.ene` file: body size, speeds, sight fan, hearing, timers. See the same section.

**Preset table (`enepcParamSets`, `EnepcParamSet`)**: Resource loaded by `enepc.readyEnepcData` with `0x30`-byte parameter sets that `enepc.setParams` picks by number. See [Field enemies: Data files](game/field-enemies.md#data-files).

**Primitive family**: Shape of a drawn point: model, screen sprite, billboard, oriented quad, fixed quad, clipped polygon. See [sef: Primitive families](libraries/sef.md#primitive-families).

**Print queue**: The deferred text records of `xgl_font.c` in 16 ordering buckets, flushed by `xglFontFlush`. See [xgl: Text](libraries/xgl.md#text). See also [Fonts and text: The print queue](libraries/fonts-and-text.md#the-print-queue).

**.prj**: The project file of a KORG bank, with the bank name and one 0x30-byte entry per sound. See [Audio: Sound banks](architecture/audio.md#6-sound-banks).

**PRJ (project)**: The project file of a bank, magic `PRJp`: three chunks `PRJp`, `KEYp` and `PRGp` that describe keys and programs over the samples of the `.vsf`. See [Sound files: PRJ projects](formats/sound.md#prj-projects).

**Probe (MapCollisionObject)**: The 0x40-byte request and result block of every collision query. See [Maps and units: The probe](game/map-and-units.md#the-probe).

**Program**: A 384-byte entry of the `PRGp` chunk, selected by a MIDI program change: a name and a note map. See [Sound files: Programs](formats/sound.md#programs).

**Proportional copy**: rows 18 to 29 of sheet 1, a second copy of glyphs 0 to 383 read by the proportional path. See [Text and fonts: The proportional copy](formats/text-and-fonts.md#the-proportional-copy).

**Proportional mode**: Drawing glyphs below index 0x180 with the left and right bounds of the `proportional_widths` table instead of the fixed 19-pixel advance. See [Fonts and text: Proportional widths](libraries/fonts-and-text.md#proportional-widths).

**pt (party system)**: The functions of `src/main/pt_member.c` and friends that keep the ten party members in slots, pick the leader, and pair E.S. with pilots. See [Party and growth: The party state](game/party-and-growth.md#the-party-state).

**pt natives**: The `menu` module natives `ptInit`, `ptClear`, `ptJoin`, `ptLeave`, `ptChangeMode`, `ptSet`, `ptSetPilot`, `ptGetLeader`, `ptSetArray`, `ptTent`, `umnReturn`. See [Party and growth: Script natives](game/party-and-growth.md#script-natives).

**ptAddMember / ptRemoveMember**: Put a member into the first free slot of its group, or remove it and close the gap. See [Party and growth: Adding and removing members](game/party-and-growth.md#adding-and-removing-members).

**ptEnterSplit / ptLeaveSplit**: Switch between on foot and E.S. mode. Entering rebuilds the roster. See [Party and growth: On foot and in the E.S.](game/party-and-growth.md#on-foot-and-in-the-es).

**ptRefresh**: Rebuilds the status block, writes E.S. crews into the parameter blocks, updates icons and applies a story-flag ether to Shion. See [Party and growth: The status block](game/party-and-growth.md#the-status-block).

**ptReset**: Party reset of `src/main/save_party_init.c`: rebuilds the ten member records, clears the party state, adds the ten members. With `menuLoadPending` set it also writes default options and loads the menu texts. See [Save data: A new game](architecture/save-data.md#a-new-game). See also [Party and growth: Reset and new game](game/party-and-growth.md#reset-and-new-game).

**ptState (PartyState)**: The live party: ten slots, ten roster entries, mode, current slots, counts and the menu cursor. See [Party and growth: The party state](game/party-and-growth.md#the-party-state).

**ptStateInit**: New-game reset behind the `ptInit` native: units, boxes, 600 points each, Jin's starting skills, the Episode I bonus. See [Party and growth: Reset and new game](game/party-and-growth.md#reset-and-new-game).

**ptStatusBlock (PartyStatusBlock)**: Digest of the party rebuilt by `ptRefresh`: fighting group ids, leader, the two masks and the seven cloth ids. See [Party and growth: The status block](game/party-and-growth.md#the-status-block).

**pxy**: The model library and its file format. A `.pxy` file is one model image that starts with the letters `pxy`. See [pxy: Model data](libraries/pxy.md#model-data).

**PXY model**: A `.pxy` file, magic `pxy\0`. Header of 0x4b0 bytes, tables, meshes as VIF streams, optional second model. See [Models and textures: PXY models](formats/models-and-textures.md#pxy-models).

**pxy profiler**: The 32-entry timer table `pxyProfile*` based on EE timer 1. See [pxy: The profiler](libraries/pxy.md#the-profiler).

**pxyFlags**: The per-frame request bits that select the extra model passes (reflection 1, glow 4, separate `0x100`, pass `0x8000`). See [Rendering: Model passes](architecture/rendering.md#model-passes).

**pxyFlush**: The function that turns the submitted models and direct entries into packet data, once per camera index 0 to 3. See [Rendering: Model passes](architecture/rendering.md#model-passes). See also [pxy: Flush](libraries/pxy.md#flush).

**PxyHeader**: Header of a `.pxy` file with counts, table offsets, bounding sphere and box. See [pxy: The .pxy file](libraries/pxy.md#the-pxy-file).

**pxySubmitModel**: The function that registers a model for the frame. See [pxy: Submitting a model](libraries/pxy.md#submitting-a-model).

## Q

**Quadword copy**: A 16-byte move with the EE instructions `lq` and `sq`, collected as macros in `include/quadword.h`. See [Math: Quadword copies](libraries/math.md#quadword-copies).

**Quaternion**: A rotation stored as (x, y, z, w) in a `Vector4`, w the scalar part. See [Math: Quaternions](libraries/math.md#quaternions).

**Quest flag**: One byte per campaign in `questFlags[36]`. Low nibble is the status, bit 0x10 the new-result mark. See [G.S. Campaign: Quest flags](game/gs-campaign.md#quest-flags).

**Quest flags**: `questFlags`, 36 bytes, natives `g2path.setQuestFlag` / `getQuestFlag` (1-based). See [Events: Quest and citizen flags](scripting/events.md#quest-and-citizen-flags).

**Quest status**: Low nibble of a quest flag: 0 not received, 1 in progress, 2 success, 3 failed. See [G.S. Campaign: Quest flags](game/gs-campaign.md#quest-flags).

**QuestMes.txt**: Tagged text file with 36 campaign records (subject, location, client, three messages, two result reports). See [G.S. Campaign: iwase\QuestMes.txt](game/gs-campaign.md#iwasequestmestxt).

**Quick preset**: a ready combination of four defocus layers set by `GameDefocusQuickSet`. See [Screen effects: Quick presets](libraries/screen-effects.md#quick-presets).

## R

**Radial blur**: effect mode 0, a zoom blur drawn as a triangle fan with random texture scales. See [Screen effects: Mode 0: radial blur](libraries/screen-effects.md#mode-0-radial-blur).

**Range test kinds**: The `kind` values 1 (front rectangle), 2 and 3 (circle), 4 to 7 (fan of 15-degree sectors) of `MMathActorsInRangeByKind`. See [Math: Range tests between actors](libraries/math.md#range-tests-between-actors).

**Rank**: In a sell list, the number of filled entries before the cursor, which is the row the cursor bar is drawn on. See [UI windows: Moving over filled items](libraries/ui-windows.md#moving-over-filled-items).

**Ratio**: `gifStatus.ratio`, the life of a `gifState` effect. It starts at 1.0 and drops by `ratioStep` each frame. See [Screen effects: Life of an effect](libraries/screen-effects.md#life-of-an-effect).

**Reach test**: `battleEffectApplies`, the test whether a technique can touch one unit (position, trait, range shape, height zone). See [Battle: The reach test](game/battle.md#the-reach-test).

**Reaction code**: The `code` of command 10: 0 and 1 plain hits, 2 Break, 3 Down, 4 Air. It selects the reaction motion and the new `state70`. See [The battle engine: Hit reactions](game/battle-engine.md#hit-reactions).

**Read control block (LW)**: State of the CD module: active read, request queue, disc state. See [xgl: Reading a file](libraries/xgl.md#reading-a-file).

**Read mode**: The flags of `xglCdReadFilePart`: `CD_READ_SYNC` (0), `CD_READ_ASYNC` (1), `CD_READ_NO_EXTRACT` (2), `CD_READ_WHILE_BUSY` (4). See [Files and resources: Reading a file](architecture/files-and-resources.md#reading-a-file).

**Read tag**: A word set with `xglCdSetReadTag` before a read and passed back to the callback as its third argument, for example a block index or a packed kind/sub pair. See [Files and resources: Reading a file](architecture/files-and-resources.md#reading-a-file). See also [xgl: The sector arena and its helpers](libraries/xgl.md#the-sector-arena-and-its-helpers).

**Receiver**: The world position a light set is built for. See [xgl: Light packs](libraries/xgl.md#light-packs).

**Record**: one `GsQueueContext` draw order of `0x7C` bytes with a type 1 to 21. See [Screen effects: Banks and the frame callback](libraries/screen-effects.md#banks-and-the-frame-callback).

**Record bank**: ten records drawn in order. There are three banks in `recordBanks`, cleared after every draw. See [Screen effects: Banks and the frame callback](libraries/screen-effects.md#banks-and-the-frame-callback).

**Record kind**: What a unit is: 1 walking character, 2 model, 3 scenery switch, 4 actor without AI (the player), 5 player part, 6 effect, 7 mover, 8 built-in model request. See [Maps and units: The record table and the kinds](game/map-and-units.md#the-record-table-and-the-kinds).

**Record pause flag**: `recordUpdatePaused`, set for one frame by `recordPauseUpdates` so that animated records hold still. See [Screen effects: Banks and the frame callback](libraries/screen-effects.md#banks-and-the-frame-callback).

**Record preset**: one of the combinations 0 to 24 that `recordSetPreset` writes into a bank. See [Screen effects: Presets](libraries/screen-effects.md#presets).

**Record table**: The table of an effect script that starts with the addresses of the six sequences and continues with the particle line entries. See [sdv: Effect scripts and their table header](libraries/sdv.md#effect-scripts-and-their-table-header). See also [Effect data files: the record table](formats/effects.md#the-record-table).

**Record type**: the first byte of a record. It selects the handler in `recordDrawHandlers`. See [Screen effects: Record types](libraries/screen-effects.md#record-types).

**RecordTexts**: The two text offsets (name, description) at the start of most `bat.dat` records. See [Party and growth: The game tables](game/party-and-growth.md#the-game-tables).

**Reference point**: The point the scripted camera looks at (`gameWork.camera.range.lookAt`). See [Native functions: The camera module](scripting/natives.md#the-camera-module). See also [Field: Direct mode and scripted cameras](game/field.md#direct-mode-and-scripted-cameras).

**Relocation (of an `.id` file)**: `UnduInit` adds the file address to every offset and sets bit `0x80` of the first byte. See [Maps and field data: Offsets and relocation](formats/maps-and-field-data.md#offsets-and-relocation).

**Render level**: The per-model option word `pxyRenderLevel` / `PxyPacket.drawFlags` set with `nmlModelSetRenderLevel`. See [pxy: Render level bits](libraries/pxy.md#render-level-bits).

**Render step**: `xglRenderMove`, the per-frame function that adds the late layers, closes the packet, waits, swaps and sends. See [Rendering: The render step](architecture/rendering.md#the-render-step). See also [xgl: The render step](libraries/xgl.md#the-render-step).

**Request buffer (`sefRequestBuffer`)**: 0x340-byte block with the actors, matrices and entries of the record that is being stepped or drawn. See [sef: The request buffer](libraries/sef.md#the-request-buffer).

**Request queue**: 32 queued asynchronous reads of `0x80` bytes each inside `LW`, started one after another by `queue_next`. See [Files and resources: Queued requests](architecture/files-and-resources.md#queued-requests).

**Requested mode**: A motion queued with `XenoActorRequestMode` that starts when the current one ends. See [Actors: Modes and slots](libraries/actors.md#modes-and-slots).

**`resApplyCfFile`**: Makes a place current: finishes the preload, frees old blocks, compacts preloaded blocks and runs every entry. See [Events: Applying a place](scripting/events.md#applying-a-place).

**`resCommandTable`**: The 13 commands of the scene resource script: `map`, `act`, `mot`, `mobj`, `effect`, `mapse`, `bgm`, `bgmvol`, `foot`, `enefoot`, `disk`, `save`, `,mpeg2`. See [Events: The commands](scripting/events.md#the-commands).

**Resident microprogram (VU0)**: The 224-pair program loaded once into VU0, 19 routines for sine, cosine and the matrix stack. See proposed text for [math: Micro mode and the resident microprogram](libraries/math.md#micro-mode-and-the-resident-microprogram).

**Resident overlay**: the overlay that stays loaded during play. OV01 in both games. See [From Episode I to Episode II: 1. Programs and overlays](architecture/from-episode-i.md#1-programs-and-overlays).

**Resident VU0 microprogram**: The program uploaded once by `xglGeometryInit`, with sine, cosine and the matrix stack routines. See [Math: Micro mode](libraries/math.md#micro-mode-and-the-resident-microprogram).

**Resource area**: The upper 16 MiB of EE memory (`0x01000000`–`0x02000000`), managed by the resource block table during a play session. See [Memory: The resource block table](architecture/memory.md#the-resource-block-table).

**Resource block**: The five addresses `GameResLoadID` returns for one object id: pack, effect (texture), hierarchy, look link, slot table. See [Actors: The resource set](libraries/actors.md#the-resource-set).

**Resource block table (`gameResBlockTable`)**: Table of 128 `GameResBlock` entries of 20 bytes that describes the resource area as consecutive blocks in address order. Entry 0 is the header. See [Memory: The resource block table](architecture/memory.md#the-resource-block-table).

**Resource manager**: The allocator and loader of `res_get_path.c` over the 16 MiB at `0x01000000`. See [Files and resources: The resource block system](architecture/files-and-resources.md#the-resource-block-system).

**Resource record table (`srsRecordTable`)**: 58 records of 28 bytes, one per loaded effect file. The record number is the image mapper number. See [sef: The resource record table](libraries/sef.md#the-resource-record-table).

**Resource set (`rsrc[]`)**: The twelve addresses of loaded files in an actor, indexed by `enum XActRsrc`. See [Actors: The resource set](libraries/actors.md#the-resource-set).

**Resource slot**: Second, simpler effect object of `sdv` (192 entries), used for script kind 0x49. Owned by the sdv page. See [sef: The boundary with sdv and MEf](libraries/sef.md#the-boundary-with-sdv-and-mef).

**Resource slot (cell slot)**: One of 192 `ResourceSlot` entries that hold a running built-in emitter effect. See [sdv: Slots, emitters and the particle pool](libraries/sdv.md#slots-emitters-and-the-particle-pool).

**Resource table**: the 128-record table over the upper 16 MiB, `GameResource[128]` in Episode I and `gameResBlockTable[128]` in Episode II. See [From Episode I to Episode II: 3. Memory](architecture/from-episode-i.md#3-memory).

**Restart position**: `restartPositionVector`, the origin, where the hidden player waits until it is placed. See [Field: The outer loop](game/field.md#the-outer-loop-one-pass-per-place).

**Retry rule**: A native that sets its status output to 1 is called again on the next run. `talk.wait`, `talk.menuAddItem`, `talk.menuGetResult` use it. See [Events: Waiting and closing](scripting/events.md#waiting-and-closing).

**Reveal**: The typewriter effect of a text slot. `reveal` moves two text units forward every `pending` frames until the end of the visible page. See [UI windows: Reveal](libraries/ui-windows.md#reveal).

**Reveal cursor**: Pointer to the first hidden byte of a text slot, advanced by `interpAdvanceCursor`. See [Fonts and text: Text slots and scrolling](libraries/fonts-and-text.md#text-slots-and-scrolling).

**Reveal position**: `MsgSlotText.reveal`, the end of the text shown so far. `talkStashPrefix` cuts the text there for printing. See [Events: Message text](scripting/events.md#message-text).

**Reward record**: 16 bytes per enemy: experience, skill points, class points, a normal and a rare drop. See [Rewards](formats/game-tables.md#rewards).

**RgHeap**: Checked best-fit heap with file/line tracking and a magic stamp per block. Six heaps selected by id. See [Memory: RgHeap](architecture/memory.md#rgheap).

**RgHeapBlock**: 64-byte block header of `RgHeap` (`pre`, `next`, `size`, `mark`, `line`, `module`, `magic`). See [Memory: RgHeap](architecture/memory.md#rgheap).

**Ribbon bands**: The three lengthwise bands into which the effect program splits an alter strip. See [pxy: The effect program](libraries/pxy.md#the-effect-program).

**Ring effect**: effect mode 1, a ring shaped distortion of 90 segments. See [Screen effects: Mode 1: ring](libraries/screen-effects.md#mode-1-ring).

**Rolling digit**: The animation of a digit of the number entry box that slides to its new value (flag 0x200, `roll_angle`). See [UI windows: Drawing the digits](libraries/ui-windows.md#drawing-the-digits).

**Roster**: `ptState.roster[10]`, the seating plan of split mode: entries 0 to 2 main pilots, 3 to 5 co-pilots, 6 to 9 reserve. See [Party and growth: The roster](game/party-and-growth.md#the-roster).

**Round robin**: the Episode I frame model, four EE threads that wake each other with `xglSleep`. Episode II replaced it with one thread. See [From Episode I to Episode II: 2. Threads and the frame](architecture/from-episode-i.md#2-threads-and-the-frame).

**Route (`points`)**: Patrol points given by the script with `enepc.setParams`. See [Field enemies: Routes and splines](game/field-enemies.md#routes-and-splines).

**Row card**: The 464 by 100 panel of one segment address with its three check icons, drawn by `ysegfDrawTreasureCard`. See [G.S. Campaign: The row card](game/gs-campaign.md#the-row-card).

**Row flags**: `gameWork.rowFlags`, two rows of 512 bytes, one per key row. 1 means the enemy group was beaten. See [Events: Pickup and enemy flags](scripting/events.md#pickup-and-enemy-flags).

**Row flags (`gameWork.rowFlags`)**: One byte per `enemyorder.dat` row (two sets of 512) that marks a beaten enemy. See [Field enemies: Removing the enemy and its companions](game/field-enemies.md#removing-the-enemy-and-its-companions).

**Row vector convention**: Vectors are rows and matrices stand on their right, so `v' = v.x*m[0] + v.y*m[1] + v.z*m[2] + v.w*m[3]`. See [Math: Matrix layout](libraries/math.md#matrix-layout-rows).

**RTPS**: Rotate, translate, perspective: the projection of a 3D point to a GS screen position. See [Math: Projection to the screen](libraries/math.md#projection-to-the-screen).

**Ruby annotation**: Small reading text centred above a base text, written with `$rubyb,`, `$rubys,`, `$rubye,`. See [Fonts and text: Ruby annotations](libraries/fonts-and-text.md#ruby-annotations).

**Ruler line**: digit line in `Treasure.txt` in front of each tag, skipped by the reader. See [Text and fonts: How the reader finds a text](formats/text-and-fonts.md#how-the-reader-finds-a-text).

**Rung**: A height on a ladder where the player can step off to the left or right. See [Maps and units: Ladders](game/map-and-units.md#ladders).

**runtime**: Module of 34 general field natives. See [Native functions: The runtime module](scripting/natives.md#the-runtime-module).

**runtime.Item**: Gives or takes items, skills and ether and can show the pickup message. See [Native functions: Items, flags and the time panel](scripting/natives.md#items-flags-and-the-time-panel).

**runtime.jumpCF**: The native that queues the transition to another map or event. See [Native functions: Objects, the player and the map](scripting/natives.md#objects-the-player-and-the-map). See also [Events: runtime.jumpCF](scripting/events.md#runtimejumpcf).

**`runtime.mapChange`**: Loads the map model with a resource file id (`mapResourceSwitch`). See [Events: Changing the map model](scripting/events.md#changing-the-map-model).

**runtime.setNumber**: The native that creates (or releases) a map object record for a key. See [Native functions: Objects, the player and the map](scripting/natives.md#objects-the-player-and-the-map).

**runtime.System**: Eleven switches on bits of `gameWork.control`. See [Native functions: Objects, the player and the map](scripting/natives.md#objects-the-player-and-the-map). See also [Events: Control bits a script can set](scripting/events.md#control-bits-a-script-can-set).

**runtimeInit**: The battle function that empties the registry and registers `samp`, `deb`, `bat`, `math`, `camera`. See [The SB virtual machine: Who registers what](scripting/sb-vm.md#who-registers-what). See also [Native functions: The battle registration](scripting/natives.md#the-battle-registration).

## S

**samp / deb**: Two test modules (3 natives) registered in both sets. See [Native functions: The samp and deb modules](scripting/natives.md#the-samp-and-deb-modules).

**Sample record**: The 40-byte header of one sample in a `.vsf`: rate, root key 60, SPU2 pitch, data size and a 16-byte source file name. See [Sound files: Sample records](formats/sound.md#sample-records).

**Samplefade**: the owner name of the script commands `ColorSet`, `Fadein`, `Fadeout`, `Colorfade`, `Colorfade2`. See [Screen effects: Script commands](libraries/screen-effects.md#script-commands). See also [Native functions: The Samplefade module](scripting/natives.md#the-samplefade-module).

**Save folder**: `/BASLUS-20892Xeno2NN` on the card, one per save number NN, holding the save file of the same name, `icon.sys`, three `.ico` files and `dummy.bin`. See [Save data: Paths and files](architecture/save-data.md#paths-and-files).

**Save image**: The contents of `SaveData` as written to a save file: header and options, script flags, thumbnail, and whole copies of live game objects. See [Save data: The save image](architecture/save-data.md#the-save-image).

**Save point symbol**: Built-in types 7 and 8. CIRCLE inside it opens the save screen. See [Maps and units: Save points and other symbols](game/map-and-units.md#save-points-and-other-symbols).

**Save thumbnail**: 88 × 77 JPEG of the screen, made by `saveThumbnailEncode`, at most `0x1000` bytes at offset `0x174` of the save image. See [section 9](formats/movies-images-and-icons.md#9-save-thumbnails).

**SAVE_VERSION**: The value 200 stored at image offset `0x50`. A save with another value is shown as damaged. See [Save data: Checksum and version](architecture/save-data.md#checksum-and-version).

**saveChecksum**: 64-bit sum over the image with the checksum field zeroed: each byte times its 1-based position, plus `0x793` per byte. See [Save data: Checksum and version](architecture/save-data.md#checksum-and-version).

**SaveData**: The save image, one `struct SaveData` object of `0x5160` bytes defined in `src/main/xgl_mc_request.c`. A memory card save file is a plain copy of it. See [Save data: The save image](architecture/save-data.md#the-save-image).

**saveDataSetDefaults**: Writes the default options into `SaveData` (sound mode 1, captions 2, the rest 0, screen position reset). See [Save data: Default options](architecture/save-data.md#default-options).

**SaveParty**: The `0x218`-byte party block of the image at `0x4EF0`: member ids per slot, split state, skill board bits, and the leader and face data shown on the save screen. See [Save data: The party block](architecture/save-data.md#the-party-block).

**saveReadGameWork**: Restores `gameWork`, the quest, citizen and segment flags and the balance from the image, and clears four control bits. See [Save data: Loading into the game](architecture/save-data.md#loading-into-the-game).

**saveReadImage**: Reverse of `saveWriteImage` for the tables and the party, called by `Game()` when `gameWork.loadStage` is not zero. See [Save data: Loading into the game](architecture/save-data.md#loading-into-the-game).

**SaveSlot**: `0x7920`-byte record per list entry: decoded 88x77 thumbnail and the summary fields (state, leader, level, faces, play time, cleared flag, place name). See [Save data: Slot records](architecture/save-data.md#slot-records).

**saveTextIndexTable**: Table of 53 map ids with two text indices into `yosikawa\Location.txd`, the place name shown for a save. See [Save data: Slot records](architecture/save-data.md#slot-records).

**saveThumbnailEncode**: Scales the draw buffer into video memory page `0x3800` and encodes it as JPEG. Called with falling quality until the result fits. See [Save data: The thumbnail](architecture/save-data.md#the-thumbnail).

**saveUiMode**: Mode of the save screen: 0 card choice, 2 slot list, 3 transfer, 4 error message. See [Save data: Modes](architecture/save-data.md#modes).

**saveWriteImage**: Function of `src/main/pt_member.c` that copies the live game state into `SaveData`, stamps the version, time and checksum. See [Save data: Writing and reading the image](architecture/save-data.md#writing-and-reading-the-image).

**SB**: The script bytecode format and its virtual machine, named after the magic `"SB  "` of a script file. See [The SB virtual machine](scripting/sb-vm.md).

**SB virtual machine (VM)**: The interpreter in `src/ov01/sb_vm.c` that runs `.sb` scripts for the field and the battle. See [The SB virtual machine: The parts of the machine](scripting/sb-vm.md#the-parts-of-the-machine).

**SB_IMAGE_LINKED**: Bit 31 of the header flags, set when an image was linked. See [The SB virtual machine: Linking](scripting/sb-vm.md#linking).

**SB_JUMP_ZERO_DIVIDE / SB_JUMP_ARGUMENTS**: The two error keys (1 and 2) that the VM and the natives raise. See [The SB virtual machine: Slots](scripting/sb-vm.md#slots).

**sbFatal**: The VM's error exit: an empty report call and an endless loop. See [The SB virtual machine: Fatal errors](scripting/sb-vm.md#fatal-errors).

**SbImageHeader**: The 0x28-byte header of an SB or SBL image. See [The SB virtual machine: The image](scripting/sb-vm.md#the-image).

**SbNativeMethod**: A (name, function) record of a native table, which ends with a null name. See [The SB virtual machine: The registry](scripting/sb-vm.md#the-registry). See also [Native functions: The registry](scripting/natives.md#the-registry).

**sbNativeOwnerAdd**: The function that copies one null-terminated native table into the registry under an owner name. See [Native functions: The registry](scripting/natives.md#the-registry).

**sbOpLengths**: The table of instruction lengths in code units. See [The SB virtual machine: Reading the code](scripting/sb-vm.md#reading-the-code).

**sbRunAll**: The once-per-frame function that runs every active thread until it stops. See [The SB virtual machine: Once per frame](scripting/sb-vm.md#once-per-frame).

**sbStep**: The dispatcher that executes one instruction through `sbOpHandlers`. See [The SB virtual machine: One thread](scripting/sb-vm.md#one-thread).

**sc**: The effect script accessors of `src/main/sc_get.c`, which find the script record of a key and resolve its offsets. See [sef: Effect scripts](libraries/sef.md#effect-scripts-sc).

**Scan tree**: A binary space tree over a mesh, built at map load from a pool of 1600 cells, used for height queries. See [Maps and units: The scan tree](game/map-and-units.md#the-scan-tree).

**Scan-line mode**: `ScanLineInterpolate` (0, 1, 2): how the two GS read circuits show the frame. Mode 1 averages neighbouring lines. See [xgl: Scan-line modes](libraries/xgl.md#scan-line-modes).

**Scanline interpolation**: `ScanLineInterpolate`, three ways to soften interlace flicker (off, read circuit blend, packet sprite). See [Rendering: Scanline interpolation](architecture/rendering.md#scanline-interpolation).

**Scene collision set (`TiGlobalCollSet`)**: One scene-wide set of spheres, planes and cylinders, plus wind curve blocks, set by the cutscene player. See [Animation: The scene collision set](libraries/animation.md#the-scene-collision-set).

**Scene list**: Byte list in `titleTableLists` that names the items of one title scene. See [section 6.5](formats/movies-images-and-icons.md#65-titlebg2bin).

**Scene resource script**: The text file `cf\cfNNNN.txt` (tag `#scn`, at most one sector) that lists the resources, sounds and events of a field scene. See [Files and resources: Scene resource scripts](architecture/files-and-resources.md#scene-resource-scripts). See also [Events: The scene resource script](scripting/events.md#the-scene-resource-script).

**Scene switch**: The byte at `sRender` +0x84 that would skip effects, models, callbacks and fade. Never set in the source. See [Rendering: The render step](architecture/rendering.md#the-render-step).

**scene thread**: The per-frame thread of a cutscene: fade in, `cfe.stFrm`, fade out, `cfe.destroy`, jump. See [A small cutscene script](formats/scripts-and-archives.md#a-small-cutscene-script).

**Scheduler**: `XglTaskScheduler`, a header plus a pool of task slots in one caller-supplied block. See [xgl: Tasks](libraries/xgl.md#tasks).

**Scheduler pass**: One call of `sefExecScheduler` over the effect records. See [sdv: The per-frame order](libraries/sdv.md#the-per-frame-order).

**`_scheduler` variable**: the name of the Episode I effect table, reused for the Episode II effect records. See [From Episode I to Episode II: 8. Effects](architecture/from-episode-i.md#8-effects).

**ScissorSet / ScissorReset**: Font callbacks in `src/main/gr_screen.c` that set and restore `SCISSOR_1` around the text of a box. See [Fonts and text: Include and callback](libraries/fonts-and-text.md#include-and-callback).

**Scratch batch**: The GS register batch built in the scratchpad by `xglPacketScratchBegin`/`AddQuad`/`PatchTag`/`AddDirect`. See [Memory: The scratchpad](architecture/memory.md#the-scratchpad).

**Scratchpad**: 16 KiB of fast EE memory at `0x70000000`, used with fixed offsets and no allocator. See [Memory: The scratchpad](architecture/memory.md#the-scratchpad).

**Scratchpad batch**: A register list built at `0x70000000` and copied into the packet as a `DIRECTHL` block (`xglPacketScratch*`, the copies of `xgl_render.c`, `gs_queue.c`). See [Rendering: Writing into a packet](architecture/rendering.md#writing-into-a-packet). See also [xgl: Scratchpad batches](libraries/xgl.md#scratchpad-batches).

**Screen effect request (cutscene)**: A kind, strength and depth from a cutscene slot that `effectRequestToCommand` turns into a record preset. See [sdv: Screen effect requests of cutscenes](libraries/sdv.md#screen-effect-requests-of-cutscenes).

**Screen effect slot (`CfeScreenEffect`)**: One of three keyed full-screen effect requests. See [Cutscenes: Screen effects](scripting/cutscenes.md#screen-effects).

**Screen fade (battle)**: The task of `fadeIn` and `fadeOut` that multiplies the frame buffer by a level from 0 to `0x80`. See [The battle engine: Screen fade](game/battle-engine.md#screen-fade).

**Screen fill**: A grey full-screen sprite drawn by a task from `screenFillStart` until `screenFillEnd`, used to hide the model swap of transform actions. See [Battle effects: The fill](game/battle-effects.md#the-fill).

**Screen flash**: The same grey sprite for about ten frames (`screenFlashStart`), used at the start and end of pair techniques. See [Battle effects: The flash](game/battle-effects.md#the-flash).

**Screen helpers**: The `gr*` functions of `gr_screen.c` for frame targets, copies, fills, fades and scissors. See [Rendering: Screen helpers](architecture/rendering.md#screen-helpers).

**Screen-sampled part**: Part with flag 0x8000 that is drawn with a copy of the screen as texture. See [pxy: The order inside a pass](libraries/pxy.md#the-order-inside-a-pass).

**ScreenVector4**: A projected point: integer x, y, z in GS format and a float w. See [Math: Types](libraries/math.md#types).

**Script flags**: The 256-byte bit field area `SaveData.scriptFlags`, read and written by `xglFlagsGet` and `xglFlagsSet`. See [xgl: Flags](libraries/xgl.md#flags).

**Script flags (scriptFlags)**: 256 bytes of packed bit fields at image offset `0x74`, addressed by bit offset and width with `xglFlagsGet` and `xglFlagsSet`. See [Save data: The script flags](architecture/save-data.md#the-script-flags).

**Script image (`.sb`, `SB  `)**: Compiled bytecode of one script: 64-byte header, code, globals, name table, imports, source name. See [SB script images](formats/scripts-and-archives.md#sb-script-images).

**Script kind**: Field of `SefScriptRecord`: 0x3C plain, 0x3D depth sorted, 0x3E group, 0x3F variant, 0x49 resource slot effect. See [sef: Finding the script of a key](libraries/sef.md#finding-the-script-of-a-key). See also [Effect data files: script kinds](formats/effects.md#script-kinds).

**scriptRegisterModules**: The field function that registers the fifteen field owners in a fixed order. See [The SB virtual machine: Who registers what](scripting/sb-vm.md#who-registers-what). See also [Native functions: The field registration](scripting/natives.md#the-field-registration).

**sdv**: The layer of the effect system that runs the timed, non-particle parts of an effect record: effect objects, shadows, wind, camera, screen filters and trails. See [sdv](libraries/sdv.md).

**SDV_KEY_END**: The frame value `0x400` that ends a key table. See [sdv: Key sequences](libraries/sdv.md#key-sequences).

**SdvSequence**: The 8-byte reader state of a key sequence: table address, elapsed passes, row position, row size. See [sdv: Key sequences](libraries/sdv.md#key-sequences).

**seBgmPlay**: The one entry point for sequenced music. Mode 0 starts a song, mode 2 starts it without touching the volume, mode 1 reloads only the bank and resumes. See [Audio: Sequenced music](architecture/audio.md#7-sequenced-music).

**Second model**: Complete model at `subPackOffset` of a map file, with flags 0x88/0x8c, for the moving pieces of a map. See [The second model of a file](formats/models-and-textures.md#the-second-model-of-a-file).

**Second pack**: The second model pack inside a map `.pxy`, whose entries are the movable and breakable objects of the map. See [Maps and field data: Map models, textures and scripts](formats/maps-and-field-data.md#map-models-textures-and-scripts).

**Second pack (sub pack)**: An optional second model inside a `.pxy` pack, drawn after the first with its own hide mask. See [Actors: Drawing](libraries/actors.md#drawing).

**Second page**: `sRender.vram.texture_base`, a second spare page used for reduced copies by the glow and the battle transition. See [Screen effects: Pages and coordinates](libraries/screen-effects.md#pages-and-coordinates).

**Secret Key**: 31 keys that open locked skills. Stored twice: table 11 (box 4) and records 77 to 107 of table 12 (box 3). See [Keys and key items](formats/game-tables.md#keys-and-key-items).

**Secret Key (menuItemCategoryMask)**: Item 1 to 31 of box 4. Bit `i` of the mask unlocks the board positions whose record has `bit == i`. See [Party and growth: Learning a skill](game/party-and-growth.md#learning-a-skill).

**Sector allocator**: `xglCdSectorAlloc`, the bump allocator of the arena, in 2048-byte sectors, valid until the arena closes. See [Main menu: Memory and files](game/main-menu.md#memory-and-files).

**Sector arena**: The bump allocator of `xgl_cd_arena.c` (`xglCdArenaOpen`, `xglCdSectorAlloc`) used by the main menu, the save screen and the battle result screen. See [Files and resources: The sector arena of the menus](architecture/files-and-resources.md#the-sector-arena-of-the-menus). See also [xgl: The sector arena and its helpers](libraries/xgl.md#the-sector-arena-and-its-helpers).

**Sector-aligned loop**: The zero filling of a looping ADX header that puts the loop start frame on a 2,048-byte boundary. It explains the header sizes `0x220` to `0x7D0`. See [Sound files: Loops](formats/sound.md#loops).

**Seek error screen**: The movie read-error prompt with replay (CIRCLE) and skip (TRIANGLE). See [Cutscenes: Playing a movie](scripting/cutscenes.md#playing-a-movie).

**sef**: The visual effect system: effect records, slots, lines and points, their keyed tracks and the per-frame update. See [sef](libraries/sef.md).

**sefDefaultMemoryBlock**: Static array of `0x152800` bytes, the large sm pool for effect files. See [Memory: The sm pools](architecture/memory.md#the-sm-pools).

**SefScriptRecord**: The header of an effect script. Its kind (`0x3C` to `0x3F`, `0x49`) decides how the start offset is found. See [sdv: Effect scripts and their table header](libraries/sdv.md#effect-scripts-and-their-table-header).

**sefSmallHeap**: Static array of `0x20000` bytes, the small sm pool for emitter ring buffers. See [Memory: The sm pools](architecture/memory.md#the-sm-pools).

**seg**: Module of 2 natives for segment addresses. See [Native functions: The g2path and seg modules](scripting/natives.md#the-g2path-and-seg-modules).

**`seg.addrOn`**: Native that stores the state of a segment address and shows one of three messages. See [G.S. Campaign: How scripts drive a segment address](game/gs-campaign.md#how-scripts-drive-a-segment-address).

**Segment**: One `textDrawSegment` call: style bytes, optional pulse colour, alpha tag, the text and closing tags. See [Fonts and text: Segments and lines](libraries/fonts-and-text.md#segments-and-lines).

**Segment address**: One of 18 numbered locked doors with a treasure behind it. Official name, also used in the code's messages. See [G.S. Campaign: The segment address screen](game/gs-campaign.md#the-segment-address-screen).

**Segment address flag**: One byte per address in `segAddrOnFlags[18]`: 0 unknown, 1 found, 2 read on the door, 3 decoded. See [G.S. Campaign: Segment address flags](game/gs-campaign.md#segment-address-flags).

**Select (actor select bits)**: A row value made of an actor bit (`0x40` source, `0x80` target, `0x100` partner, `0x400`/`0x800`/`0x1000` links) plus a part index plus one. See [sdv: Actor select bits](libraries/sdv.md#actor-select-bits).

**Selected index**: `gameResBlockTable[0].owner`, the first block that allocation and the scene reset may touch. Blocks below it are permanent. Set by `resSelectSlot`. See [Files and resources: Allocating and freeing](architecture/files-and-resources.md#allocating-and-freeing).

**Selection**: The object in reach that carries the marker and that SQUARE attacks, stepped with L1 and R1. See [Field: Attacking map objects](game/field.md#attacking-map-objects).

**Selection box**: The glowing textured box around the selected digit, eased between digits through `cursor_x`. See [UI windows: Drawing the digits](libraries/ui-windows.md#drawing-the-digits).

**selectionFlagTable**: Flag word per usable item id (restore HP, restore EP, always, restricted, disabled, E.S. only, any mode, Class Points, Skill Points). See [Party and growth: Who can use what](game/party-and-growth.md#who-can-use-what).

**SelectionRecord**: The 6-byte record of one skill: kind, value for the battle code, cost, required bit. See [Main menu: Skills](game/main-menu.md#skills).

**SelectionRecord / record index**: Six-byte record per board position (kind, number, cost, key). Index is `group * 32 + slot * 4 + sub`. See [Party and growth: The record table](game/party-and-growth.md#the-record-table).

**Selector**: The number of the current face animation (`XenoActorSetSelector`). See [Actors: Play range, speed and face animation](libraries/actors.md#play-range-speed-and-face-animation).

**Sell window**: The list window with bound name, cost and count tables of 40 entries, the labels `:Sell`, `:Close` and `Remaining Debt:`. See [UI windows: The sell window](libraries/ui-windows.md#the-sell-window-items-costs-and-counts). See also [G.S. Campaign: What the window shows](game/gs-campaign.md#what-the-window-shows).

**seMaxDistance**: The distance beyond which a positioned sound is silent: 10 for on-foot characters, 60 for the three other player groups. See [Audio: Looping sounds](architecture/audio.md#9-looping-sounds-and-positional-sound).

**Separate batch**: Batch of a model with render level bit 0x40, drawn after the scene with a cleared depth buffer. See [pxy: Separate batches](libraries/pxy.md#separate-batches).

**seStateRestore**: Rebuilds banks, music and looping voices of the field, used after battles and quest screens. See [Audio: Looping sounds](architecture/audio.md#9-looping-sounds-and-positional-sound).

**seTonePlay**: Plays one footstep on unit 1 with a rotating channel, one of two notes and a random pitch bend. See [Audio: Footsteps](architecture/audio.md#10-footsteps).

**Setup batch**: Batch type 1. It carries the material: GS registers, texture values, flags, scroll and specular values, and the material kind. See [pxy: The setup batch and the material](libraries/pxy.md#the-setup-batch-and-the-material).

**Setup context**: `gifStateSetupContext`, the frame copy and GS state that modes 0 to 4 start with. See [Screen effects: The setup context](libraries/screen-effects.md#the-setup-context).

**sev**: Prefix of the point allocator functions (`sevAllocPtAllocator` and others). See [sef: The point allocator](libraries/sef.md#the-point-allocator-sev).

**Shadow caster mode**: Frame setting (`pxySetShadowCasterMode`) that chooses which receiver parts take projected shadows. See [pxy: Projected shadows](libraries/pxy.md#projected-shadows).

**Shadow mode**: Value of `pxySetShadowMode`: 1, 4, 5 projected, 2 circle, 3 planar. See [pxy: Shadows](libraries/pxy.md#shadows).

**Shadow projector (`PxyShadow`)**: The matrices, texture registers and box of one projected shadow. See [pxy: Projected shadows](libraries/pxy.md#projected-shadows).

**Shadow sequence**: The sequence that writes the shadow fields of the source or target actor (record field `shadowSeq`). See [sdv: The shadow sequence](libraries/sdv.md#the-shadow-sequence).

**Shadow type (`drawMode`)**: Selects a projected shadow of the model library or a ground shadow of `act_3.c`. See [Actors: Shadows](libraries/actors.md#shadows).

**Shake key**: A camera key that starts up to three camera waves. See [sdv: Shake keys](libraries/sdv.md#shake-keys).

**Shared names**: function names defined in both source trees, counted by family. See [From Episode I to Episode II: 11. What carried over unchanged](architecture/from-episode-i.md#11-what-carried-over-unchanged).

**Shared unit resources**: `mapObjectResources[54]`: four data files and 25 model and texture pairs from `matumoto\`, loaded once per field session. See [Maps and units: Shared unit resources](game/map-and-units.md#shared-unit-resources).

**Shown balance and target balance**: `msgShownBalance` is the number on screen, `msgTargetBalance` the real debt after the sales. The shown one runs down by 100 per frame. See [G.S. Campaign: Selling](game/gs-campaign.md#selling).

**SIF**: The channel between EE and IOP, used through `sceSif*`. See [Runtime libraries: The IOP link](architecture/runtime-libraries.md#the-iop-link).

**Sight fan**: The cone (`fanWidth`, `fanLength`) in which an enemy sees the player, tested by `Check_InsideFan` with map and map unit occlusion. See [Field enemies: Sight](game/field-enemies.md#sight).

**simajiri**: Disc directory of the effect files. See [sef: Load modes and paths](libraries/sef.md#load-modes-and-paths). See also [From Episode I to Episode II: 8. Effects](architecture/from-episode-i.md#8-effects).

**Simulation node step**: The seven-step update of one cloth or hair node in `ti_dynamics_node.c`. See [Math: The simulation node step](libraries/math.md#the-simulation-node-step).

**Single thread frame**: the Episode II frame, where `xglSleep` calls `xglRenderFrame`, `xglCdControlThread` and `xglStudioUpdate` one after the other. See [From Episode I to Episode II: 2. Threads and the frame](architecture/from-episode-i.md#2-threads-and-the-frame).

**Single-byte character**: A byte 0x80-0xFF that is not the start of one of the two-byte pairs accepted by `xglFontIsTextureGlyphCode`. It selects glyph 256 + (byte − 0x80). See [Fonts and text: One-byte and two-byte characters in the font](libraries/fonts-and-text.md#one-byte-and-two-byte-characters-in-the-font).

**Skill (status table)**: A record of `skillRecordGet` (104 records). Learned skills are the `flagBits` of the point record, set by the misleadingly named `dataEtherLearnSet`. See [Party and growth: Skills and ethers in the source names](game/party-and-growth.md#skills-and-ethers-in-the-source-names).

**Skill board**: The 112 skills of a character in 4 levels, 28 classes and 4 skills per class. See [Main menu: Skills](game/main-menu.md#skills).

**Skill board (level, class, skill)**: Four levels (groups) of eight classes (slots, four on level 4) of four skills (subs), the same for every character. See [Party and growth: Skills](game/party-and-growth.md#skills).

**Skill board model**: The round 3D model of the skill board made of plates and balls. See [Main menu: The skill board](game/main-menu.md#the-skill-board).

**Skill mask (menuItemCategoryMask)**: The bit mask of skill groups that are available. A skill outside it is shown as text 400. See [Main menu: Set-up in menuSceneInit](game/main-menu.md#set-up-in-menusceneinit).

**Skill points (S.Pt)**: The points that buy a skill (`PartyMemberExpPoints.skillPoints`). See [Main menu: Skills](game/main-menu.md#skills). See also [Party and growth: Learning a skill](game/party-and-growth.md#learning-a-skill).

**Skill record (`SkillRecord`)**: 12-byte record of a passive effect, used by skills, accessories and enemies. See [Skills and equipment effects](formats/game-tables.md#skills-and-equipment-effects).

**Skill sum**: `calcSkill(unit, flagMask, codeMask)`, the sum of the values of all skill and equipment records that match two masks. See [Battle: Skills and equipment in the rules](game/battle.md#skills-and-equipment-in-the-rules).

**Skill tutorial**: The scripted demonstration of the skill board, played from `Tutorial.txd` on a temporary board. See [Main menu: The skill tutorial](game/main-menu.md#the-skill-tutorial).

**Skip node / face node (`TiHrcSkip`)**: A node driven by the face motion, with rotation limits. See [Animation: Face nodes](libraries/animation.md#face-nodes).

**skip_hidden**: Flag of `GameMpeg2Play` set for movie names starting with `EP`. Such a movie cannot be skipped. See [section 2.1](formats/movies-images-and-icons.md#21-where-they-are-and-who-plays-them).

**Slerp**: Spherical interpolation of two quaternions, `XrgQuaternionSlerp`. See [Math: Blending two matrices](libraries/math.md#blending-two-matrices).

**Slide**: The cosine-eased move of the rows of a list window after a scroll (flag 0x20). See [UI windows: Cursor, sweep and scroll](libraries/ui-windows.md#cursor-sweep-and-scroll).

**Slide block**: `SlideBlock`, a `0x80`-byte block with position, angle, velocity, acceleration and goal. HUD panels, units and the cursor move through it. See [The battle engine: The slide block](game/battle-engine.md#the-slide-block).

**Slide position**: Per-row horizontal offset of the segment list (`ysegfEntryPos`) that eases to 16, 30 or 512 and also gives the row opacity. See [G.S. Campaign: The list page](game/gs-campaign.md#the-list-page).

**Slope and conveyor ground**: Ground of class `0x10000` that scales or forces the player's movement. See [Maps and units: Slopes](game/map-and-units.md#slopes).

**Slot (camera)**: One of four timed transitions: look-at, position, field of view, roll. See [Field: Slots and transitions](game/field.md#slots-and-transitions).

**Slot (OV01)**: An entry of one of the six fixed object tables of the battle overlay. Every slot starts with a flags word, an update callback and a draw callback. See [The battle engine: The object model](game/battle-engine.md#the-object-model).

**Slot (`SefEffectEntry`)**: One of the 32 parts of an effect record. It starts lines at given frames and sends flags to actors. See [sef: Slots](libraries/sef.md#slots).

**Slot attachment**: Attaching a child at a global matrix index of the parent (`XenoActorAttachAtSlot`). The battle uses slot 27. See [Actors: Parents, children and attachments](libraries/actors.md#parents-children-and-attachments).

**Slot entry**: A 10-byte `SefEffectLoadEntry`: start frame, line list offset, 32 flag bits, link byte, signal byte. See [Effect data files: the record table](formats/effects.md#the-record-table).

**Slot flags**: Bits of `MsgSlot.flags`: active 0x1, closing 0x2, opening 0x4, open 0x8, and type-specific animation bits above. See [UI windows: Slot flags](libraries/ui-windows.md#slot-flags). See also [Events: Showing the message](scripting/events.md#showing-the-message). See also [The battle engine: The object model](game/battle-engine.md#the-object-model).

**Slot transition**: The enter or leave animation of a control slot, a direction flag 1 to 4 and a counter from 128 to 0, drawn through `grDrawScreenFade`. See [Main menu: Transitions](game/main-menu.md#transitions).

**Slot type (`MsgSlot.state`)**: 1 text, 2 list window, 3 timecode, 4 item notice. It selects the body and the update and draw handlers and never changes while the slot lives. See [UI windows: MsgSlot](libraries/ui-windows.md#msgslot).

**Slots (front and back)**: `ptState.slots[10]`. Slots 0 to 2 are the battle party on foot, 3 to 6 the reserve, 7 to 9 the E.S. See [Party and growth: Slots](game/party-and-growth.md#slots).

**sm pool**: Best-fit pool of the effect system with 64-byte units and `SmCell` headers (`smAlloc`, `smFree`). Two pools: `smPool` and `smPoolSmall`. See [Memory: The sm pools](architecture/memory.md#the-sm-pools).

**Small glyphs**: four strips of 64 glyphs of 8 by 8 texels at texel rows 736 to 760 of sheet 1. See [Text and fonts: Small glyphs](formats/text-and-fonts.md#small-glyphs).

**Small glyphs (8x8 strips)**: Four strips of 64 glyphs of 8 by 8 texels at V 736 to 760 of the font texture: kana, hex digits and ASCII. See [Fonts and text: Small glyphs](libraries/fonts-and-text.md#small-glyphs).

**SmCell**: 16-byte block header of an sm pool, tag `0x1234` (free) or `0x5678` (used). See [Memory: The sm pools](architecture/memory.md#the-sm-pools).

**Snap**: Camera flag `0x80`, reset the probes and cut to the new view. See [Field: The camera update](game/field.md#the-camera-update).

**Snapshot**: A JPEG of the frame buffer, with an optional APP14 segment of metadata and a 128 by 112 thumbnail. See [Field: Snapshots](game/field.md#snapshots).

**Sofdec (`mwPly*`)**: CRI's movie player middleware. See [Runtime libraries: The Sofdec movie player](architecture/runtime-libraries.md#the-sofdec-movie-player).

**Sofdec movie (`.sfd`)**: CRI movie file, an MPEG-1 system stream of 2,048-byte packs with one MPEG-1 video stream, an ADX stream and an AC-3 stream. See [Movies, images and icons: Sofdec movies](formats/movies-images-and-icons.md#2-sofdec-movies).

**SofdecStream header**: Private stream 2 packet in pack 2 with the file name, multiplexing date, multiplexer version, stream counts, lengths in milliseconds and the frame count. See [section 2.3](formats/movies-images-and-icons.md#23-the-sofdec-header-packs).

**Soft outline**: Post-process around a model with status bit 0x80000 (`nmlModelSetDropPass`). See [pxy: The soft outline](libraries/pxy.md#the-soft-outline).

**Soft reset**: L1, L2, R1, R2 and Select held with Start pressed. Sets `CONTROL_SOFT_RESET` (`0x10000000`) and every loop returns until `main()` shows the title again. See [Boot and title: The soft reset](game/title-and-boot.md#15-the-soft-reset). See also [Battle: The scene loop](game/battle.md#the-scene-loop).

**Soft reset combination**: L1 + L2 + R1 + R2 + SELECT held with START pressed (`PAD_SOFT_RESET`). See [xgl: The pad record](libraries/xgl.md#the-pad-record).

**Soft-float**: libgcc routines (`dpadd`, `dpcmp`, `fptodp`, `dptofp` ...) that implement `double` on the single-precision EE. See [Runtime libraries: The compiler runtime](architecture/runtime-libraries.md#the-compiler-runtime).

**Solver body (`TiSolverState`)**: The moving state of a dynamics node: previous position, position, velocity accumulator, gust factor. See [Animation: What a dynamics node is](libraries/animation.md#what-a-dynamics-node-is).

**Solver passes (`solverPasses`)**: Number of times blend, chaining and dynamics run in the next update. Used to settle hair after a jump in position. See [Animation: tiMRPackageUpdate](libraries/animation.md#timrpackageupdate).

**Song**: Sequenced music: `sound\smf\<name>.vsf`, `.prj` and `.mid`. The name starts with a three-digit song number. See [Audio: Sequenced music](architecture/audio.md#7-sequenced-music).

**Song bank**: The `.vsf`/`.prj` pair next to a `.mid` in `sound\smf\`. Several programs, loaded into slot 0, at most 399,808 bytes of sample data. See [Sound files: Bank names](formats/sound.md#bank-names).

**Sort table**: Table of `PxySortEntry` records between +0xe8 and +0xec, used by one model (`gno019`). See [The sort table](formats/models-and-textures.md#the-sort-table).

**Sorted batch list**: Linked list of batches ordered from far to near, from `pxyFirstBatch` to `pxyLastBatch`. See [pxy: The batch lists](libraries/pxy.md#the-batch-lists).

**Sound bank**: A `.vsf` sample file plus a `.prj` project file with the same name, loaded into a KORG bank slot. See [Audio: Sound banks](architecture/audio.md#6-sound-banks).

**Sound layer**: The game's own sound code between game code and the two libraries: `sound_driver.c`, `se_bank_load.c`, `se_volume.c` and parts of `res_get_path.c`. See [Audio: Layers](architecture/audio.md#2-layers).

**Sound memory address**: The place in the sound chip's memory the game assigns to a bank (`seBankAddressTable`, third argument of `seBankLoad`). See [Audio: Sound banks](architecture/audio.md#6-sound-banks).

**Sound memory slot**: The fixed address range of SPU2 memory that the game gives to a bank slot, from `seBankAddressTable`. See [Sound files: Banks and sound memory](formats/sound.md#banks-and-sound-memory).

**Sound record**: An entry of kind `0x90000` whose text starts with a bank number or one of the letters `f`, `e`, `s`, `v`. See [Events: The commands](scripting/events.md#the-commands).

**Sound slot (ADX slot)**: One of the two ADX players (`SoundSlot`). Slot 0 plays music, slot 1 voices. See [Audio: ADX streams](architecture/audio.md#11-adx-streams).

**soundApplySettings**: The per-frame sound function: master ramp, MIDI flush, slot ramps, `ADXM_ExecMain`. See [Audio: Sound in a frame](architecture/audio.md#13-sound-in-a-frame).

**soundCtrl**: The 48-byte sound control block (`SoundControl`): song number, four default levels, the master ramp and two ADX slots. See [Audio: The sound control block](architecture/audio.md#4-the-sound-control-block).

**soundMode**: The mono/stereo option in the save data, three values, 0 is mono. See [Audio: Output settings](architecture/audio.md#19-output-settings).

**soundRestart / soundReset**: Restart both ADX slots, pause the music player and silence all notes. `soundReset` also unmutes and stops the music. See [Audio: Pause, menus and scene changes](architecture/audio.md#14-pause-menus-and-scene-changes).

**soundSetMuted / seSetMuted**: The small mute (units 1 to 3) and the big one (also pauses the music player and removes the effect). See [Audio: Volume](architecture/audio.md#12-volume).

**Source (`CD_SOURCE_*`)**: Where a file is read from: disc (0), host (1) or HDD (2). `0xFF` marks an unused archive slot. See [Files and resources: From a path to a position](architecture/files-and-resources.md#from-a-path-to-a-position).

**Source chain**: A DMA transfer that follows `CNT`, `REF` and `END` tags in memory. The frame packet is one source chain on channel 1. See [Rendering: DMA channels](architecture/rendering.md#dma-channels).

**Source name**: The string at header word 0x28, the path of the script source (`nakahara\sc1010`, `think106`). Not read by the game. See [Source name](formats/scripts-and-archives.md#source-name).

**Source name (`thinkNNN`)**: The string at header word 10 of a think script image, the name it had in the unused directory `yamamoto\think\`. See [Where the scripts live](formats/game-tables.md#where-the-scripts-live).

**Source pass**: Draw mode 4, an early pass for models or parts marked with status bit 0x20000 or part flag 0x10000. See [pxy: One camera](libraries/pxy.md#one-camera).

**Spawn record (`TgtSlot`)**: Five words: unit key, position, degrees, think entry, camera entry. See [Enemy groups](formats/game-tables.md#enemy-groups).

**Spawn slot (`SefSpawnSlot`)**: One of 32 burst spawners that scatter copies of a field effect over a box for some frames. See [sef: Burst spawning](libraries/sef.md#burst-spawning).

**Speaker name**: `MsgSlotText.speaker_name`, copied up to `]` by `talkCopySpeakerName` and drawn as a name plate. See [Events: Message text](scripting/events.md#message-text). See also [Text and fonts: Speaker names](formats/text-and-fonts.md#speaker-names).

**Speaker name plate**: The name printed above the dialogue window, taken from a bracket tag at the start of the fed text. See [UI windows: Feeding text](libraries/ui-windows.md#feeding-text).

**Speaker tag**: `[name]` at the start of a message, stored as the speaker name of a text slot. See [Fonts and text: Button tags in message windows](libraries/fonts-and-text.md#button-tags-in-message-windows).

**Special**: A screen filter that belongs to an effect, played from a special table. See [sdv: Specials: screen filters](libraries/sdv.md#specials-screen-filters).

**Special action**: A technique with `hitFlags` bit 0 that runs C code instead of normal hits: cure all, item pick, analyze, steal, transform. See [Battle: Special actions](game/battle.md#special-actions).

**Special attack record (`SpecialAttackRecord`)**: The four KOS-MOS weapons MINIGUN, MICRO MISSILE, DRAGON BLADE, X-BUSTER. See [Special attacks of characters](formats/game-tables.md#special-attacks-of-characters).

**Special record**: `SdvSpecialWork`, one of 32 shared running filters. See [sdv: Structures](libraries/sdv.md#structures).

**Special table**: `SdvSpecialTable`: kind, part, modes and a track of 26-byte keys. See [sdv: Structures](libraries/sdv.md#structures). See also [Effect data files: special rows and special tables](formats/effects.md#special-rows-and-special-tables).

**attackRecordTable**: 88 `LookupRecord` entries the menu uses for attack lists: per-member attacks, co-pilot attacks and the 16 Double Tech records. See [Party and growth: Pilots and co-pilots](game/party-and-growth.md#pilots-and-co-pilots).

**Specular exponent count**: The integer k in lane w of 0x3c1. The highlight term is squared k + 1 times. See [pxy: Lighting](libraries/pxy.md#lighting).

**Speed byte**: The `signal` byte of a slot. It sets the global effect speed in percent at the slot's start frame. See [sef: Child effects](libraries/sef.md#child-effects).

**Speed profile**: Up to 32 weighted segments that shape a move or a turn (`objx.setStepMove`, `Get_MiddlePoint_Curve`). See [Maps and units: Tweens and the objx natives](game/map-and-units.md#tweens-and-the-objx-natives).

**Spiral**: Fire type 2: eight blue ribbons winding along a curve between two actors. See [Battle effects: Spiral (type 2)](game/battle-effects.md#spiral-type-2).

**spline.dat**: 1,345 rows of four 16-bit B-spline weights (sum 24,576) in five blocks of 269 rows. Only block 2 is read. See [Maps and field data: spline.dat](formats/maps-and-field-data.md#splinedat).

**`spline.dat` (`DataSpline`)**: Weight table of the B-spline, 2048 phases of four 16-bit weights from byte 4304. See [Field enemies: Data files](game/field-enemies.md#data-files).

**Split**: A program whose note map sends ranges of notes to different keys, a multi-sampled instrument. 20 programs. See [Sound files: Programs](formats/sound.md#programs).

**Split mode**: The party mode in which the three E.S. are on screen (`ptEnterSplit`). Owned by the party page. See [Field: Starting a session](game/field.md#starting-a-session). See also [Party and growth: On foot and in the E.S.](game/party-and-growth.md#on-foot-and-in-the-es).

**Spray**: Fire type 8: twenty glowing streaks that fall from height 30 to the ground around the scene origin. See [Battle effects: Spray (type 8)](game/battle-effects.md#spray-type-8).

**Sprite cell**: One numbered picture of a loaded `.xtx` texture, drawn with `menuDrawCell`. See [Main menu: The text and sprite layer](game/main-menu.md#the-text-and-sprite-layer).

**SPU2 ADPCM**: The 16-byte block format (28 samples of 4 bits, a filter and shift byte, a flags byte) that the PS2 sound chip decodes, the format of Sony VAG files. See [Sound files: SPU2 ADPCM](formats/sound.md#spu2-adpcm).

**sRender**: The render state at `0x00645820`: resolution row, VRAM row, the two swapped buffer words, 17 callback slots and counters. See [Rendering: The render state](architecture/rendering.md#the-render-state). See also [xgl: The render state](libraries/xgl.md#the-render-state).

**srs**: The effect file layer: effect keys, stems, file kinds, load modes and the resource record table. See [sef: Effect files and the resource table](libraries/sef.md#effect-files-and-the-resource-table).

**srsAtan2**: The table arctangent of the effect library (1024 steps), `srsAtan2(x, y)` measured from the second argument. See [Math: Trigonometry](libraries/math.md#trigonometry).

**Standard MIDI File (SMF)**: The `.mid` song format. All 31 songs are format 0 with 480 ticks per quarter note. See [Sound files: Songs](formats/sound.md#songs-standard-midi-files).

**Standard motions (`walk[16]`)**: Per-character table of motion numbers for standing, walking, running, discovery and threat, replaced with `enepc.defaultMotion`. See [Field enemies: The standard motions](game/field-enemies.md#the-standard-motions).

**Stat block**: `StatBlock`, the numbers of one unit for one battle (HP, EP, stats, statuses, Stock, zones). See [Battle: The stat block](game/battle.md#the-stat-block).

**Stat panel**: A short label that slides in on a panel row for 15 frames to announce a result of a hit. See [The battle engine: Hit panels](game/battle-engine.md#hit-panels).

**stat.xtx**: The single HUD texture of the battle. See [The HUD texture](formats/game-tables.md#the-hud-texture-statxtx).

**StatBlock (parameter block)**: `0x108`-byte block of a unit's parameters. `battleSlotTable` holds the persistent ones for keys 1 to 15. See [Party and growth: The parameter block](game/party-and-growth.md#the-parameter-block).

**Static node**: A `TiHrcStatic` record of 0x40 bytes, a fixed transform under a joint that child actors are attached to. In version 2.4 it carries an `id`. See [Skeletons and motion: Static nodes](formats/skeletons-and-motion.md#static-nodes).

**Static node (`TiHrcStatic`)**: A fixed transform with an id, used as an attachment joint for child actors. See [Animation: Node records](libraries/animation.md#node-records).

**Status aura**: One of three long-running effects of group 0x12C that `statEffProc` keeps on a unit with a status. See [Battle effects: Other senders](game/battle-effects.md#other-senders).

**Status bits**: The word `nmlRenderStatus` / `PxyPacket.renderStatus` that collects per-model requests and results. See [pxy: Status bits](libraries/pxy.md#status-bits).

**Status code (`MSG_STATUS_*`)**: 0 to 5, the classification of a slot returned by `msgWindowCurrentStatus` and `msgSlotStatus`. See [UI windows: Status codes](libraries/ui-windows.md#status-codes).

**Status record (HUD)**: The panel of one party unit at the top of the battle screen: face, HP, EP and level pips or gauge. See [The battle engine: Status records](game/battle-engine.md#status-records).

**Status resistance bytes**: 16 bytes per status row at 0x38 (row 1) and 0x48 (row 2) of a unit record, read by `calcStatResist`. See [Unit records](formats/game-tables.md#unit-records).

**Status row**: One of the four 16-bit status masks of a unit: rows 1 and 2 harmful, rows 3 and 4 helpful and modifiers. See [Battle: Status effects](game/battle.md#status-effects).

**Status word**: The second parameter of a native. Setting it to 1 makes the VM repeat the call in the next frame. See [The SB virtual machine: The call](scripting/sb-vm.md#the-call).

**Stem**: Text form of an effect key, such as `ce000` or `ce000_051`, written by `srsFormatEffectStem`. See [sef: Stems and names](libraries/sef.md#stems-and-names). See also [Effect data files: stems and naming families](formats/effects.md#stems-and-naming-families).

**Step (battle)**: A function `void step(struct SaveStack *)` that does one piece of scene work and names its successor with `ov01StepSetNext`. See [The battle engine: The step stack](game/battle-engine.md#the-step-stack).

**Step chain**: The 21 step functions `ov01Step*` that load, run and close a battle, one step per frame, each naming its successor with `ov01StepSetNext`. See [Battle: The step chain](game/battle.md#the-step-chain).

**Step icon**: The 24 by 42 HUD sprite at (348, 386) whose picture follows `actStepCounterValue()`. See [The battle engine: Stock icons and the step icon](game/battle-engine.md#stock-icons-and-the-step-icon).

**Step sampling**: `cfeTrackSampleStep`, the value of the last key at or before the time. See [Cutscenes: Sampling](scripting/cutscenes.md#sampling).

**Step stack (SaveStack)**: The object that runs the battle scene as a chain of step functions. Its current step sits at the offset of a slot's update callback, so the update sweep calls it each frame. See [The battle engine: The step stack](game/battle-engine.md#the-step-stack).

**Stock**: `StatBlock.level`, 0 to 3 saved attacks of a character. See [Battle: Stock](game/battle.md#stock).

**Stock icons**: Five small HUD objects that show the units of the action stock in acting order. See [The battle engine: Stock icons and the step icon](game/battle-engine.md#stock-icons-and-the-step-icon).

**Stock stance**: Unit flag 0x4000, set by the Stock or Charge command until the unit's next turn. See [Battle: Stock and Charge](game/battle.md#stock-and-charge).

**Story group**: Group 10 or 20, the cutscene motion, movies and voice streams of one half of the story (scenes S-01-03-00 to S-03-20-00 and S-04-02-00 to S-10-02-00). See [Groups and pieces](formats/scripts-and-archives.md#groups-and-pieces).

**Story progress flag**: The 8-bit game flag at bit offset `0x191`. Picks the disc, the synopsis picture and other details. See [Events: Game flags](scripting/events.md#game-flags). See also [Boot and title: Starting a session: the synopsis](game/title-and-boot.md#9-starting-a-session-the-synopsis).

**Story step script**: A script without a map (`sc7001` to `sc7099` and a few others) that sets flags, plays a movie or changes the party and jumps on. See [Scripts without a map](formats/scripts-and-archives.md#scripts-without-a-map).

**STORY_PROGRESS_FLAG**: The 8-bit script flag field at bit `0x191`, the story progress value. See [Save data: The script flags](architecture/save-data.md#the-script-flags).

**Stream**: `CdStreamParam`, a file read piecewise into a ring buffer. See [xgl: Streams](libraries/xgl.md#streams).

**Stream (battle data)**: A `DataStreamEntry` with the `.pxy` and `.txy` of a battle map, 2 MiB or `0x2A800` bytes. See [The battle engine: Streams: battle maps](game/battle-engine.md#streams-battle-maps).

**Stream (`CdStreamParam`)**: A file read piece by piece into a ring buffer by the per-frame service. Only the IPU movie player of `xgl_1.c` uses it. See [Files and resources: Streams](architecture/files-and-resources.md#streams).

**Stream C0 / C1 / E0**: The ADX sound, the AC-3 sound and the video of a Sofdec file. See [section 2.6](formats/movies-images-and-icons.md#26-the-audio-streams).

**Stream ring (`StreamRing`)**: Buffer, capacity, write position and read position of a stream. See [Files and resources: Streams](architecture/files-and-resources.md#streams).

**Stride**: The number of halfwords in one row of a key sequence (7 effect, 7 shadow, 11 wind, 2 special, 28 alter). See [sdv: Effect scripts and their table header](libraries/sdv.md#effect-scripts-and-their-table-header).

**Strike (search action)**: SQUARE on a selected target: a motion chosen by the target's level, events 0 and 1, and a break when the target has flag `0x40000`. See [Maps and units: Striking](game/map-and-units.md#striking).

**String argument (type 3)**: An argument whose type word is 3 (`SCRIPT_ARG_STRING`) and whose value is a key into the library image. See [Native functions: Strings, arrays and data](scripting/natives.md#strings-arrays-and-data).

**String key**: `0x200000 + index`, the value by which a script names a string of its library. Always pushed with opcode 0x06. See [Keys](formats/scripts-and-archives.md#keys).

**String library (.sbl)**: SB image with magic `SBL ` holding the strings of one script, index at 0x40. See [Text and fonts: The container](formats/text-and-fonts.md#the-container).

**Strip**: One 16-pixel-high row of macroblocks of an IPU movie frame. See [xgl: Playing a frame](libraries/xgl.md#playing-a-frame). See also [Fonts and text: Cells](libraries/fonts-and-text.md#cells).

**Strip restart flag**: Lowest bit of the bit pattern of the v coordinate of a vertex. A flagged vertex starts a new triangle strip. See [Vertex batches](formats/models-and-textures.md#vertex-batches).

**Strip tag table**: The 16 GIF tags at 0x35c, indexed by the bits untextured, blended, five registers and fog. See [pxy: The setup batch and the material](libraries/pxy.md#the-setup-batch-and-the-material).

**Strip text**: Menu lettering drawn from the letter and digit strips of the base sheet by `textDrawGlyphString` and its relatives. See [Fonts and text: Strip text](libraries/fonts-and-text.md#strip-text).

**Studio**: A set of eight cameras and one light block (`0x3180` bytes). There are four. See [xgl: Studios](libraries/xgl.md#studios). See also [Field: Studios and studio cameras](game/field.md#studios-and-studio-cameras).

**Stun (`Enemy_Damage_Explosion`, `Enemy_Stiff`, `cooldown`)**: Actions 9 and 6: 30 frames of reaction, then frozen until `cooldown` (300 per blast) runs out. Contact during the stun adds a bonus bit to the battle. See [Field enemies: Explosions and the stun](game/field-enemies.md#explosions-and-the-stun).

**Style label table (`textStyleTable`)**: The titles of battle boxes by index: ETHER, ITEM, DOUBLE, SPECIAL, W ETHER, empty, STATUS, EXPLANATION. See [UI windows: Choice menus](libraries/ui-windows.md#choice-menus).

**Sub-window values**: the four integers `gifStatus.subWindow[4]` set by `nmlModelUseSubWindow`, read differently by each effect mode. See [Screen effects: The status block](libraries/screen-effects.md#the-status-block).

**Surface number**: The low byte of a footstep code. See the same section.

**Surface pass, reflection pass (water)**: The two passes of a water sheet. The first shows the screen copy bent by 0.05 times the wave vector. The second adds brightness x + y (1 - |d|)^4. See [pxy: The water program](libraries/pxy.md#the-water-program).

**Suspended thread**: A thread with `SB_THREAD_SUSPENDED` (bit 31 of `status`), skipped by `sbRunAll`. See [The SB virtual machine: Starting and ending](scripting/sb-vm.md#starting-and-ending).

**sv layer**: The drawing back end of the effect system in `sv_get_image_item.c`. See [pxy: The sv layer](libraries/pxy.md#the-sv-layer).

**svSceneFlags**: A 32-bit flag word with test, set, clear and toggle functions. Not saved. See [Save data: Scene flags](architecture/save-data.md#scene-flags). See also [pxy: Scene flags and small helpers](libraries/pxy.md#scene-flags-and-small-helpers).

**Sway**: Fire type 10: five `missile.pxy` models that spread, then swirl outward. See [Battle effects: Sway (type 10)](game/battle-effects.md#sway-type-10).

**Sweep**: The cosine-eased move of the cursor bar of a list window to another row (flag 0x100). See [UI windows: Cursor, sweep and scroll](libraries/ui-windows.md#cursor-sweep-and-scroll).

**Switch (opcode 0x25)**: The variable-length instruction that jumps through a sorted table with a binary search. See [The SB virtual machine: The switch](scripting/sb-vm.md#the-switch).

**Synopsis**: The chapter picture shown while a loaded session starts (`GameSynopsis`), held until Circle (`GameSynopsisCrossfadeWait`). See [Boot and title: Starting a session: the synopsis](game/title-and-boot.md#9-starting-a-session-the-synopsis).

**Synopsis panel (`GameSynopsisPanel`)**: A raw 512 by 64 image from `synopsis.bin` drawn at the bottom of the synopsis. The outgoing panel cross-fades to the incoming one. See [Boot and title: Starting a session: the synopsis](game/title-and-boot.md#9-starting-a-session-the-synopsis).

**Synopsis picture**: One of `yajima\synopsis\01.jpg` to `22.jpg`, a full page with the heading "The Story So Far..." and the summary text painted in. See [section 5.1](formats/movies-images-and-icons.md#51-the-pictures).

**Synopsis screen**: `GameSynopsis`, the screen at the start of a play session that checks the disc and shows a picture and two text panels chosen by the story progress. See [Events: The synopsis screen](scripting/events.md#the-synopsis-screen).

**synopsis.bin**: Archive of the two status panels of the synopsis screen ("Loading..." and "button: Start"), stored as `XTX` textures. See [section 6.4](formats/movies-images-and-icons.md#64-synopsisbin).

**SYS_SE**: The system sound bank (menu cursor, confirm, cancel), bank slot 1 at `0x5000`, loaded once in `main()`. See [Audio: Start-up](architecture/audio.md#3-start-up). See also [Sound files: Example: SYS_SE.vsf](formats/sound.md#example-sys_sevsf).

**System header**: Packet `BB` in packs 0 and 1 that lists the streams and their buffer sizes. See [section 2.2](formats/movies-images-and-icons.md#22-the-program-stream).

**system.ico**: The only icon file of a save folder, written from `carddata.bin` offset `0x440`. See [section 8.1](formats/movies-images-and-icons.md#81-layout).

## T

**Tab line**: The labels under the cards that select a sub-page, driven by `menuTabCursor`. See [Main menu: The card row and the command row](game/main-menu.md#the-card-row-and-the-command-row).

**Table offset header**: The first 0x54 bytes of `batdat.us`: count 19, file size, 19 offsets from the start of the file. See [Header](formats/game-tables.md#header).

**EnemyRewardRecord (enemy reward record)**: 16 bytes per enemy key 16 to 143: experience, Skill Points, Class Points, two drops with chances. See [Party and growth: Collecting rewards](game/party-and-growth.md#collecting-rewards).

**Tag**: The second word of an argument. Low byte 0 means an empty (omitted) argument, 3 means a string. See [The SB virtual machine: Arguments are pairs](scripting/sb-vm.md#arguments-are-pairs).

**Tagged background read (`xglCdReadFileAsync`)**: An asynchronous read whose tag is a kind/sub pair (font slot, background layer, skill model, menu picture) that `xglCdReadDoneCallback` dispatches on. See [Files and resources: The sector arena of the menus](architecture/files-and-resources.md#the-sector-arena-of-the-menus).

**Tagged text file**: The format of the three `iwase\*.txt` files: lines of `Tag:"string"` read in a fixed order by `yg2PathReadTaggedText`, Shift-JIS converted to EUC. See [G.S. Campaign: The tagged text files](game/gs-campaign.md#the-tagged-text-files). See also [Text and fonts: Tagged text files](formats/text-and-fonts.md#tagged-text-files).

**Tagged text table**: The record files of `iwase\` (`QuestMes.txt`, `Citizen.txt`, `Treasure.txt`). See [The three text tables](formats/game-tables.md#the-three-text-tables).

**talk**: Module of 16 natives for conversations, choices and shop windows. See [Native functions: The talk module](scripting/natives.md#the-talk-module).

**Talk result**: `talkResult`, the id of the record the player addressed. Set by `talkSetResult`, read once by `talk.check`. See [Events: Finding the partner](scripting/events.md#finding-the-partner).

**Talk target**: The box, door or person that CIRCLE acts on (`uwamonoFindTalkTarget`). See [Maps and units: Talking, opening and examining](game/map-and-units.md#talking-opening-and-examining).

**`talk.menuDeptQuest` / `talk.menuDeptGetResult`**: The two natives that open the sell window and poll its result (the remaining debt). See [G.S. Campaign: Opening the sell window](game/gs-campaign.md#opening-the-sell-window).

**`talk.smsg` / `talk.msgDtl`**: The natives that open a message window with script text and values. `msgDtl` adds a slot flag word. See [Events: Showing the message](scripting/events.md#showing-the-message).

**`talk01.xtx`**: The texture `omiya\talk01.xtx` with the frame pieces of the message windows (handle `talkTextureHandle`). See [UI windows: The window frame](libraries/ui-windows.md#the-window-frame). See also [Text and fonts: The talk window sheet](formats/text-and-fonts.md#the-talk-window-sheet).

**Tangent angle**: The slope of a curve at a key, stored as an angle in radians in a value-over-seconds diagram. See [Animation: Evaluating a curve](libraries/animation.md#evaluating-a-curve). See also [Skeletons and motion: Keys](formats/skeletons-and-motion.md#keys).

**Tangent directions**: The two 2-D unit vectors (time, value) of a key that shape the Bezier segment on each side. See [Cutscenes: Tangents](scripting/cutscenes.md#tangents).

**Target flags**: `TecRecord.flags`, the bit mask that says which side and which units a technique is for. See [Battle: Target flags](game/battle.md#target-flags).

**Target state**: `UwamonoRecord.state`: 0 out of reach, 1 selected, 2 in reach. See [Maps and units: Targets](game/map-and-units.md#targets).

**Task**: A per-frame callback kept in a 128-byte slot of a scheduler. Almost unused in this game. See [xgl: Tasks](libraries/xgl.md#tasks).

**Task (OV01)**: One of 32 records of `ov01TaskTable`, claimed with `ov01TaskClaim`, for short jobs such as fades, the screen flash and the message object. Tasks are drawn last. See [The battle engine: The object model](game/battle-engine.md#the-object-model).

**Technique**: Any action with a hit and damage formula. 779 records of 0x20 bytes (`TecRecord`). See [Technique records](formats/game-tables.md#technique-records).

**Technique class**: `TecRecord.gateFlag`: 0 normal attack, 1 double technique, 2 once-per-battle ether, 3 combined ether, 4 E.S. special, 5 special with model, 6 to 8 other attacks, 10 energy drain. See [Battle: Technique](game/battle.md#technique).

**Technique class (`gateFlag`)**: Byte 0x1C of a technique record, which fills the id ranges exactly. See [Technique records](formats/game-tables.md#technique-records).

**Technique id ranges**: 1 to 40 character attacks, 41 to 70 E.S., 71 to 80 special attacks, 81 to 96 Double Techs, 97 to 629 enemies, 631 to 670 items, 671 to 779 ethers. See [Shared conventions](formats/game-tables.md#shared-conventions).

**Technique record**: `TecRecord`, the 0x20-byte record of `bat.dat` that defines an action: targets, range, hit thresholds, power, attributes, zones, status, class. See [Battle: Technique](game/battle.md#technique).

**TecRecord**: A `0x20`-byte technique record of the battle data, returned by `tecRecordGet`. See [The battle engine: The battle data getters](game/battle-engine.md#the-battle-data-getters).

**Tepodon**: Fire type 5: the model `tepodon.pxy` flying in an arc with a smoke trail. See [Battle effects: Tepodon (type 5)](game/battle-effects.md#tepodon-type-5).

**Text arguments**: Ten values per text slot, inserted where the script text has `%d` (`msgWindowSetSlot`). See [UI windows: Feeding text](libraries/ui-windows.md#feeding-text).

**Text block (FontTextBlock)**: 0xD4-byte unit of a box, a scroll track, a layout record and a font object. See [Fonts and text: Text blocks and the typewriter layout](libraries/fonts-and-text.md#text-blocks-and-the-typewriter-layout).

**Text box**: One of eight selection boxes of `src/main/text_box.c` with a four-slot stack of text blocks and a highlighted line. Apparently unused in the finished game. See [Fonts and text: Text box stacks](libraries/fonts-and-text.md#text-box-stacks).

**Text cursor record (`CursorTextRecord`)**: A 0xD8-byte pool record of `text_box.c` that pairs a list cursor with the playback state of a text block and a font object. `cursor_buffer.c` holds its pad handling. See [UI windows: Text cursor records](libraries/ui-windows.md#text-cursor-records).

**Text entry (TextEntry)**: One of four paged text windows of the battle text layer. See [Fonts and text: Page markers](libraries/fonts-and-text.md#page-markers). See also [UI windows: Text entries](libraries/ui-windows.md#text-entries).

**Text output buffer**: A 6000-byte stack buffer in `src/main/msg_slot_core.c` that holds the formatted text of all open text slots. See [UI windows: Feeding text](libraries/ui-windows.md#feeding-text).

**Text pair (`RecordTexts`)**: Two signed halfwords at the start of a menu record, the offsets of its name and description from the start of its own table. See [Shared conventions](formats/game-tables.md#shared-conventions).

**Text slot**: A message window slot in state `MSG_SLOT_TYPE_TEXT`, driven by `interpUpdate`. See [Fonts and text: Text slots and scrolling](libraries/fonts-and-text.md#text-slots-and-scrolling). See also [UI windows: Text slots](libraries/ui-windows.md#text-slots-the-dialogue-window).

**Text tag (TextTag)**: Entry of the 20-entry `textTagTable` that remembers where a button picture belongs and from which reveal position it shows. See [Fonts and text: Button tags in message windows](libraries/fonts-and-text.md#button-tags-in-message-windows).

**Text window (TextWindow)**: The single sliding one-line bar of the battle screen, opened by `textWindowShow`. See [Fonts and text: The text window](libraries/fonts-and-text.md#the-text-window). See also [UI windows: The text window](libraries/ui-windows.md#the-text-window).

**Texture animation (frame list)**: List of `TEX0_1` writes at the end of a material batch, one per frame. See [Material batches](formats/models-and-textures.md#material-batches).

**Texture area**: The 512 KiB of VRAM from page `0x1C0` (block `0x3800`) shared by all texture users. See [Rendering: Textures](architecture/rendering.md#textures).

**Texture cache**: `pxyTextureTableCache` and `pxyTextureChannelCache`, which skip a model texture upload when the same set is already in the texture area. See [Rendering: Textures](architecture/rendering.md#textures).

**Texture cache, matrix cache, model cache**: Three one-entry caches that avoid repeated uploads. See [pxy: The caches](libraries/pxy.md#the-caches).

**Texture record**: 0x60-byte record with base block, format, size, position, palette block, set, mipmap level and name of one texture. Not read by the game. See [Texture records](formats/models-and-textures.md#texture-records).

**Texture set**: The uploads and texture records of one layer. See [Texture sets](formats/models-and-textures.md#texture-sets).

**Texture sheet**: The 1024 by 512 (8-bit), 1024 by 1024 (4-bit) or 512 by 256 (32-bit) picture that every `TEX0` of a model declares. A texture is the corner of the sheet at its base block. See [How a model finds its texels](formats/models-and-textures.md#how-a-model-finds-its-texels).

**Texture slot (font object)**: One of six buffers that hold the eleven `yosikawa\*.xtx` sheets, several sheets per slot. States 0 empty, 1 loading, 2 loaded. See [Fonts and text: Textures and slots](libraries/fonts-and-text.md#textures-and-slots).

**Texture state record**: A 16-byte `TexStateRecord` from which `grPacketTexState` writes `TEX0`, `ALPHA`, `TEST`, `CLAMP` and `TEX1`. See [The battle engine: Texture states, sprites and numbers](game/battle-engine.md#texture-states-sprites-and-numbers).

**Texture transfer set**: `XglTextureTransSet`, a list of 20-byte entries uploaded by `xglPacketTextureTrans`. See [Rendering: Textures](architecture/rendering.md#textures). See also [xgl: Texture transfer](libraries/xgl.md#texture-transfer).

**Texture window**: Four floats (u offset, u scale, v offset, v scale) that give the place of a texture inside its sheet in sheet units. See [The material table](formats/models-and-textures.md#the-material-table).

**Think**: The decision of a script-controlled unit: message kind 1 to its script entry, answered with `bat.thinkSet`. See [Battle: Enemy thinking](game/battle.md#enemy-thinking).

**Think end**: `CMD_KIND_THINK_END`, the command that closes a turn and queues the status, defeat and revive events. See [Battle: Think end](game/battle.md#think-end).

**Think entry**: Named script entry `<stem>_think` that receives the think message of one enemy type. See [Entry names](formats/game-tables.md#entry-names).

**Think machine**: the Episode I enemy AI interpreter `cmd.c`, replaced by SB scripts. See [From Episode I to Episode II: Battle](architecture/from-episode-i.md#battle).

**Think script**: The battle script in `gameWork.resBuffer`, with its library at `gameWork.imageEnd`. See [The SB virtual machine: The think script](scripting/sb-vm.md#the-think-script). See also [Battle scripts](formats/game-tables.md#battle-scripts-the-think-scripts).

**Thread (SbThread)**: One 0x40-byte record of `sbThreadTable` with a program counter, a stack, eight jump slots and a message queue. See [The SB virtual machine: Threads](scripting/sb-vm.md#threads).

**Thread entry (value)**: The code position a thread started at. With the key it is the thread's identity. See [The SB virtual machine: Identity](scripting/sb-vm.md#identity).

**Thread key**: The image address, by which the engine names all threads of one script. See [The SB virtual machine: The parts of the machine](scripting/sb-vm.md#the-parts-of-the-machine).

**Thumbnail**: JPEG of the screen (88x77, at most `0x1000` bytes) stored at image `0x174`. See [Save data: The thumbnail](architecture/save-data.md#the-thumbnail).

**Thunder25, Thunder31**: Fire types 3 and 4: lightning heads with spark models and glow quads that jump along a sampled curve (25 or 31 samples). See [Battle effects: Thunder25 (type 3)](game/battle-effects.md#thunder25-type-3).

**Ti library**: The animation library of the engine (`Ti*` files in `src/main/`): curves, motion solver, blending, dynamics, wind, water. See [Animation](libraries/animation.md).

**Tick counter**: `xglTickCounter`, a 16-bit frame counter for menu animation, advanced by `xglTickAdvance()` in the loops of 2D screens. See [xgl: The tick counter](libraries/xgl.md#the-tick-counter). See also [Fonts and text: Tracks](libraries/fonts-and-text.md#tracks). See also [Battle effects: Pause and the tick counter](game/battle-effects.md#pause-and-the-tick-counter).

**Time chart player**: `PlayControl` of Episode I, replaced by the `cfe` key tracks. See [From Episode I to Episode II: Cutscenes](architecture/from-episode-i.md#cutscenes).

**Time panel**: A countdown in `gameWork.panelValue` shown as seconds and frames, sending event 7 at 0. See [Field: The time panel](game/field.md#the-time-panel).

**Timecode slot**: Slot type 3, a 64-bit value shown as digits (mode 0) or as a clock (modes 1 and 2) at 30 frames per second. See [UI windows: Number entry and the timecode counter](libraries/ui-windows.md#number-entry-and-the-timecode-counter).

**Timed phase**: A technique motion played from the second motion table of the actor in mode slot 1 (`unitSetTimedPhase`). See [The battle engine: Phases are motions](game/battle-engine.md#phases-are-motions).

**Timer panel**: The countdown `gameWork.panelValue` opened by `runtime.TimeAttack`. Sends event 7 at 0. See [Events: Events sent to scripts](scripting/events.md#events-sent-to-scripts).

**Title demo scene**: The real-time 3D scene behind the title picture, 13 cuts played from `yajima\titlebg2.bin`. See [Boot and title: The demo scene](game/title-and-boot.md#the-demo-scene).

**Title loop**: The `for (,,)` of `main()`: load OV02, call `Title()`, decode its return value, load the named overlay and call the named function. See [Boot and title: The title loop](game/title-and-boot.md#3-the-title-loop-and-the-title-return-value).

**Title picture**: `yajima\title.bin` on disc 1, `yajima\title2.bin` on disc 2. A raw 512 by 448 image drawn over the demo scene. See [Boot and title: Loading](game/title-and-boot.md#loading). See also [section 7](formats/movies-images-and-icons.md#7-the-title-pictures).

**Title return value (selection)**: 32-bit word, top byte the overlay number, low 24 bits the entry address. 0 means stay on the title. See [Boot and title: The title loop](game/title-and-boot.md#3-the-title-loop-and-the-title-return-value).

**`Title()`**: The title screen function of OV02. See [Boot and title: The title screen](game/title-and-boot.md#6-the-title-screen).

**`titleActorPlacements`**: Ten position and rotation rows for the actors of the demo scene. See [Boot and title: The demo scene](game/title-and-boot.md#the-demo-scene).

**titlebg2.bin**: Archive of 190 model, texture, skeleton, motion and camera items for the 13 scenes behind the title menu. See [section 6.5](formats/movies-images-and-icons.md#65-titlebg2bin).

**`titleFadeTransition(mode)`**: Fade of the saved frame to black after a menu entry is confirmed (mode 0, 8 frames). See [Boot and title: Drawing](game/title-and-boot.md#drawing).

**TitleGameInit**: OV02 function that clears `gameWork`, the script flags, the party, the quest paths and the balance before New Game and Load Game. See [Save data: A new game](architecture/save-data.md#a-new-game).

**`TitleGameInit()`**: Clears `gameWork`, the script flags, the menu and party state, the G.S. paths and the balance before New Game and Load Game. See [Boot and title: New Game and Load Game](game/title-and-boot.md#7-new-game-and-load-game).

**`TitleHddInstall(command)`**: Dispatcher of the HDD dialogs (install, uninstall, four message screens). Unreachable in this release. See [Boot and title: HDD install and uninstall](game/title-and-boot.md#8-hdd-install-and-uninstall).

**`titleobj.xtx`**: Texture with the menu entries of the title screen, strips of 120 by 18 texels. See [Boot and title: Loading](game/title-and-boot.md#loading).

**`titleState`**: The state block of the title screen (cursor, HDD stage, fades, menu slots, viewer pan). See [Boot and title: State](game/title-and-boot.md#state).

**`titleTable`**: The state block of the title demo scene (scene state, cut number, record table, camera record, frames, lights). See [Boot and title: State](game/title-and-boot.md#state).

**`titleTableReadNotify`**: Disc read callback that relocates the record table and counts the records that have arrived, so the scene starts while the file still streams. See [Boot and title: The demo scene](game/title-and-boot.md#the-demo-scene).

**`TM_Check_Before_Eyes`**: Short look ahead of a character for another character or missing ground. See [Field enemies: Sight](game/field-enemies.md#sight).

**Together (`together[4]`)**: Up to four enemy records that are removed as one group when one of them is beaten, set by `enepc.setTogether`. See [Field enemies: Removing the enemy and its companions](game/field-enemies.md#removing-the-enemy-and-its-companions).

**Total (calcTotalParaMenu)**: A working copy of a parameter block with item, equipped-skill and learned-skill bonuses applied. What every menu reads. See [Party and growth: Totals for the menu](game/party-and-growth.md#totals-for-the-menu).

**`total_size`**: Header field of a `.vsf` at `0x0C`, the sum of all sample data sizes and so the room the bank needs in sound memory. See [Sound files: Header](formats/sound.md#header).

**Track**: A keyed animation value stored in the model (`PxyTrackRecord`, `PxyTrackEntry`), advanced at submit time and sent to VU1. See [pxy: Tracks](libraries/pxy.md#tracks). See also [sef: Tracks and key sampling](libraries/sef.md#tracks-and-key-sampling). See also [Tracks](formats/models-and-textures.md#tracks).

**Track (FontTrack)**: A 0x1C-byte two-dimensional value that eases toward a target, used for positions, sizes and fades. See [Fonts and text: Tracks](libraries/fonts-and-text.md#tracks).

**Track end key**: A key with frame 0x400 (`SEF_TRACK_END_KEY`), which ends a track. See [sef: Key formats](libraries/sef.md#key-formats).

**Trail (`trailPoints`)**: The list of points a character walks at the moment, filled from the script route or by `Homing_Search`. See [Field enemies: Routes and splines](game/field-enemies.md#routes-and-splines).

**Transform header (`TiXacTransformHeader`)**: The 4-byte (8-byte for dynamics) head of a node block in a curve stream: nine channel bits, a mode bit, a weight. See [Animation: Curve streams and transform headers](libraries/animation.md#curve-streams-and-transform-headers).

**Transition / event timer**: `eventState`, `eventStateArg`, `eventTimer`, `event` on the player record. `DispMapObject` counts the timer down and then writes `nextMap`. Event kind 1 is the on-foot / E.S. change. See [Events: runtime.jumpCF](scripting/events.md#runtimejumpcf).

**Translation row**: Row 3 of a matrix (byte offset 48), which holds the position. See [Math: Matrix layout](libraries/math.md#matrix-layout-rows).

**Trap**: Built-in types 0 to 3. Breaking one creates a trap point that puts nearby enemies into action 9 for 300 frames. See [Maps and units: Traps](game/map-and-units.md#traps).

**Trap point**: One of 16 `uwamonoPoints`: position, lifetime, radius. See [Maps and units: Traps](game/map-and-units.md#traps).

**Travel mode**: `camera->state`: 0 off, 1 fixed, 2 free, 3 look-at, 4 game. See [xgl: Travel modes](libraries/xgl.md#travel-modes).

**Treasure box**: Built-in types 4 to 6, opened with CIRCLE. The lid tilts 120 degrees and the content flies to the player. See [Maps and units: Treasure boxes](game/map-and-units.md#treasure-boxes).

**Treasure flag**: Script flag `0x1C1 + n - 1`, set when the treasure of address *n* was taken. See [G.S. Campaign: Flags that the segment screen borrows](game/gs-campaign.md#flags-that-the-segment-screen-borrows).

**Treasure.txt**: Tagged text file with 18 treasure records (item, location, explanation). See [G.S. Campaign: iwase\Treasure.txt](game/gs-campaign.md#iwasetreasuretxt).

**treasurebox.dat**: 513 rows of 8 bytes (type, id, count) that say what each pickup gives. The row number is also the flag index. See [Maps and field data: treasurebox.dat](formats/maps-and-field-data.md#treasureboxdat).

**Triangle clipper**: Map program, material flag 0x800: triangles are cut against five planes at the guard band of 0x3e7. See [pxy: The Map program](libraries/pxy.md#the-map-program).

**Triangle record (`MapRecord`)**: 16 bytes: attribute index, three neighbour indices, three vertex indices, plane index. See [Maps and field data: Triangles](formats/maps-and-field-data.md#triangles).

**Turn machine**: `tgtSelectStep` with its four stages begin, activate, cursor and execute in `BattleState.flags`. See [Battle: The turn machine](game/battle.md#the-turn-machine).

**Tutorial.txd**: The text table that holds the script of the skill tutorial. See [Main menu: The skill tutorial](game/main-menu.md#the-skill-tutorial).

**Tween**: A timed change of position, rotation, scale or transparency of a map object (`mapObjectAdvanceTweens`). See [Field: The map model and the map objects](game/field.md#the-map-model-and-the-map-objects). See also [Maps and units: Tweens and the objx natives](game/map-and-units.md#tweens-and-the-objx-natives).

**Two-layer vertex**: Five quadwords per vertex (two coordinate sets, two colors, one position) kicked twice with tags whose no-operation registers hide the other layer. See [pxy: Two layers](libraries/pxy.md#two-layers).

**Two-pass load**: The loader's way to read a group of files: pass 0 collects the sizes, pass 1 starts the reads at a moving cursor. See [The battle engine: Reading files](game/battle-engine.md#reading-files).

**TXD string table**: A `.txd` file: 32-bit offsets followed by zero-terminated strings, read by `txdTableEntry`. See [TXD string tables](formats/models-and-textures.md#txd-string-tables).

**TXD table**: offset array plus NUL-terminated strings, no count field (`MainMenu.txd`, `Tutorial.txd`, `Location.txd`). See [Text and fonts: Menu string tables](formats/text-and-fonts.md#menu-string-tables).

**.txd table**: String table: one 32-bit offset per string from the start of the file, then the strings. No count. See [The .txd text tables](formats/game-tables.md#the-txd-text-tables).

**txdTableEntry**: The lookup of one string in a `.txd` table: `table + table[index]`. See [Main menu: Files the menu reads](game/main-menu.md#files-the-menu-reads).

**txy**: The texture file of a model. A `TxyBlock` lists texture sets, one per layer. See [pxy: The .txy texture block](libraries/pxy.md#the-txy-texture-block).

**TXY texture file**: A `.txy` file, magic `txy\0`: header of 0x90 bytes with 32 set offsets, texture sets, upload data. See [TXY texture files](formats/models-and-textures.md#txy-texture-files).

**tyaDisplaySetting**: Applies, resets or interactively adjusts the screen position saved at `0x32` and `0x34`. See [Save data: The header and the options](architecture/save-data.md#the-header-and-the-options).

**Type word**: The second stack word of a native argument: 0 omitted, 1 integer, 3 string, 4 thread, 5 array, pushed with opcodes 0x27, 0x28, 0x2A, 0x2B, 0x2C. See [Type words](formats/scripts-and-archives.md#type-words).

**Typewriter layout**: `fontObjLayoutString`, which prints a text cut at a growing character limit and handles `\n`, `\k`, `\s`. See [Fonts and text: The layout function](libraries/fonts-and-text.md#the-layout-function).

## U

**U.M.N. page**: The page that lists up to eight earlier places and starts the travel there. See [Main menu: The U.M.N. page](game/main-menu.md#the-umn-page).

**umn strip**: One of the eight 448 × 32 JPEG strips `yosikawa\umnN.jpg` of the main menu. See [section 5.1](formats/movies-images-and-icons.md#51-the-pictures).

**Undu (undulation data)**: The collision block of a map and the code that queries it. See [Maps and units: The collision mesh](game/map-and-units.md#the-collision-mesh).

**Undu (undulation)**: The code's name for the collision mesh and its queries. See the same section.

**UnduCheck**: The single collision entry point: height under a point, or a move with stop or slide. See [Maps and units: UnduCheck step by step](game/map-and-units.md#unducheck-step-by-step).

**Unit (KORG unit)**: An independent group of 16 MIDI channels with its own volume in the KORG driver. Unit 0 music and master, 1 footsteps, 2 and 3 map banks, 4 everything else. Not to be confused with a battle unit or a map unit. See [Audio: Voices](architecture/audio.md#8-voices-sound-effects-as-midi-messages).

**Unit command**: An `Ov01Command` in a unit's command list. Its number selects one of 40 handlers of `unitCommandTable`. See [The battle engine: The unit command list](game/battle-engine.md#the-unit-command-list).

**Unit fade**: A task that changes the alpha of a unit's actors over ten frames (`unitFadeIn`, `unitFadeOut`) or to a given level (`unitFadeTo`). See [The battle engine: Fades](game/battle-engine.md#fades). See also [Battle effects: Unit fades](game/battle-effects.md#unit-fades).

**Unit flags**: `UwamonoRecord.flags`: shown, falling, targetable, breakable, E.S. only, dormant and others. See [Maps and units: Unit flags](game/map-and-units.md#unit-flags).

**Unit index**: The index of a unit in the combined unit table (0 to 7), the number battle scripts use for a unit. See [Battle: Units, sides and keys](game/battle.md#units-sides-and-keys).

**Unit key**: `StatBlock.key`, the `bat.dat` record number of a unit. 1 to 10 characters, 11 to 15 E.S., 16 to 140 enemies, 141 to 143 transformed units. `key < 16` is the party side test. See [Battle: Units, sides and keys](game/battle.md#units-sides-and-keys). See also [The battle engine: Unit keys](game/battle-engine.md#unit-keys). See also [Shared conventions](formats/game-tables.md#shared-conventions).

**Unit models**: The 25 loaded `.pxy`/`.txy` pairs of `matumoto\` (traps, treasure boxes, save points, pickups). See [Maps and field data: Unit models](formats/maps-and-field-data.md#unit-models).

**Unit record**: 0x5C-byte record of `batdat.us`, the head of a `StatBlock`: name offset, HP, EP, six stats, speed, level, traits, break sequence, attribute reactions, status resistances. See [Unit records](formats/game-tables.md#unit-records).

**Unit size**: The number of positions per row of a list cursor. In a choice menu it is the number of columns. See [UI windows: ListCursor](libraries/ui-windows.md#listcursor).

**Unit state panel**: The HUD label at (16, 80) that names the Break, Down or Air state of a unit. See [The battle engine: Hit panels](game/battle-engine.md#hit-panels).

**Unit table**: `unitTbl`, eight unit pointers: three for the player side and five for the enemy side. See [The battle engine: The unit table](game/battle-engine.md#the-unit-table).

**Unit traits**: The fixed bits of `StatBlock.flags` (type, immunities to Air and Down, never guards, never evades, counter gauge, untargetable). See [Battle: Unit traits](game/battle.md#unit-traits).

**Unit type**: The built-in model number of a unit: 0 to 3 traps, 4 to 6 treasure boxes, 7 to 9 save symbols, 10 and 11 return symbols, 12 and up items. See [Maps and units: Shared unit resources](game/map-and-units.md#shared-unit-resources).

**UNIT_FLAG_ACTIVE**: Bit `0x1` of a unit's flags, set while a command sequence runs. The battle rules wait for it to fall. See [The battle engine: The unit object](game/battle-engine.md#the-unit-object).

**UNIT_FLAG_DOWN**: Bit `0x4` of a unit's flags: the unit is defeated and left out of draws and target lists. See [The battle engine: The unit object](game/battle-engine.md#the-unit-object).

**unitBindActors**: The function that copies the file buffers of a data slot into the resources of a unit's actors and attaches the children. See [The battle engine: Binding the files](game/battle-engine.md#binding-the-files).

**unitRunCommands**: The per-frame runner of a unit's command list. A handler returns 1 when done and 0 to be called again next frame. See [The battle engine: The unit command list](game/battle-engine.md#the-unit-command-list).

**unitSetLevel**: Debug function that recomputes a unit at any level from its `bat.dat` record and growth record. See [Party and growth: The growth curves](game/party-and-growth.md#the-growth-curves).

**Update callback**: the per-actor function of Episode I (`enemy->update`). See [From Episode I to Episode II: 7. Actors and animation](architecture/from-episode-i.md#7-actors-and-animation).

**Upload record**: 0x20-byte record (`XglPacketRecord`) of one rectangle of 32-bit pixels with its destination block. See [Upload records](formats/models-and-textures.md#upload-records).

**uwamono (module)**: Module of 5 natives for doors, breakable objects and signals. See [Native functions: The uwamono module](scripting/natives.md#the-uwamono-module).

**uwamonoAttachCurrent / uwamonoDetachCurrent**: Send the party to one of eight fixed maps and back, remembering map, position and split mode in the progress record (saved). See [Save data: Attaching the party to another map](architecture/save-data.md#attaching-the-party-to-another-map). See also [Main menu: The U.M.N. page](game/main-menu.md#the-umn-page).

**`UwamonoBody`**: The pair of pointers (actor, behaviour block) at `+0x330` of a map unit record. See [Field enemies: Enemies, NPCs and the player](game/field-enemies.md#enemies-npcs-and-the-player).

**uwamonoChangePlayerModel**: Creates the player or swaps its model. Request -1000 changes form. See [Field: Creating and changing the player](game/field.md#creating-and-changing-the-player).

**uwamonoEventIds**: The eight thread entries of the map script that receive field events, set by the native `runtime.message`. See [The SB virtual machine: Messages](scripting/sb-vm.md#messages).

**`uwamonoMoveAlongHeading`**: One step of a character with map sweep, map unit test and character test. See [Field enemies: Moving one step](game/field-enemies.md#moving-one-step).

**uwamonoPlayerUpdate**: The per-frame player update: input, motion, targets, collision. See [Field: The player update](game/field.md#the-player-update).

**UwamonoRecord**: The 0x870-byte record of a unit. See [Maps and units: The record](game/map-and-units.md#the-record).

**`uwamonoSendEvent`**: Queues an `SbMessage` (kind = slot, two arguments) on the script thread registered for a slot. See [Events: Events sent to scripts](scripting/events.md#events-sent-to-scripts).

**uwamonoSetTransition**: Queues a place change on the player record with a timer and a fade-out. See [Field: Changing place](game/field.md#changing-place).

**UwamonoState**: One of 32 state blocks (0x40C0 bytes) owned by actor units, holding AI and route data. See [Field enemies](game/field-enemies.md).

## V

**Variant table**: The offset list of a kind 0x3F script, one record table per low-four-bit sub number. Only in `default.esp`. See [Effect data files: script kinds](formats/effects.md#script-kinds).

**Vector key / scalar key / integer key**: The three key layouts (`SefTrackKey16` 8 words, `SefTrackKey` 4 words, `SefKeyframe` 4 words). See [Effect data files: key tracks](formats/effects.md#key-tracks).

**Vector4**: The engine's only vector type, four floats x, y, z, w, also used for colours, Euler angles, quaternions and planes. See [Math: Types](libraries/math.md#types).

**Vertex batch**: Batch with a header (`0x8000 + n`, end mark, vertex type) and the arrays of up to 30 strip vertices. See [Vertex batches](formats/models-and-textures.md#vertex-batches). See also [pxy: A vertex batch](libraries/pxy.md#a-vertex-batch).

**Vertex type**: Header word of a vertex batch: bit 0 = a color per vertex, the rest = skin code 0, 1, 2 or 4. See [Vertex batches](formats/models-and-textures.md#vertex-batches).

**Vibration**: Small motor pulses (`Vibration_Set_Weak`) and large motor ramps (`Vibration_Set_Strong`) driven by `VibrationEngine`. See [Field: Vibration](game/field.md#vibration).

**VibrationEngine**: The per-frame vibration step: a countdown for the small motor and a linear fade for the large motor. See [Audio: Vibration](architecture/audio.md#20-vibration).

**View matrix**: `volume[0].viewMatrix`, projection times world-to-camera, built by `xglCameraViewVolume`. See [xgl: Building the view](libraries/xgl.md#building-the-view).

**View path**: A folder name set with `srsSetViewPath` and inserted into host, USB and debug effect paths. See [Files and resources: Effect files](architecture/files-and-resources.md#effect-files).

**VIF1 interval timer**: Stopwatch in the VIF1 interrupt handler that measures the VIF1/VU1 time of a frame in timer 1 ticks (`intrTimerAccum`). See [xgl: The VIF1 interval timer](libraries/xgl.md#the-vif1-interval-timer).

**VIF1 markers**: The two `FLUSH` VIFcodes with interrupt bit per packet. The VIF1 handler times the span between them (`intrTimerAccum`). See [Rendering: Interrupts](architecture/rendering.md#interrupts).

**Virtual pad**: Script-fed pad input in battle (`bat.VPadEnable`, `bat.VPadSet`). See [Native functions: Effects, messages and the HUD](scripting/natives.md#effects-messages-and-the-hud). See also [Battle: The virtual pad](game/battle.md#the-virtual-pad).

**Visibility table**: An `XacCurveKeyTable` behind the streams of a version 1.4 entry, a key count and pairs of frame and value. Table n shows or hides geometry n of the model. See [Skeletons and motion: Visibility tables](formats/skeletons-and-motion.md#visibility-tables).

**Visibility table (`XacCurveKeyTable`)**: Step keys in a motion that hide or show one model part. See [Animation: Visibility curves](libraries/animation.md#visibility-curves).

**vmArgGet**: Reads the value word of a 1-based argument. A bad index is fatal. See [Native functions: Arguments on the stack](scripting/natives.md#arguments-on-the-stack).

**vmArgGet / vmArgGetHigh / vmArgTagIsZero**: The readers of an argument's value, tag and emptiness. See [The SB virtual machine: Arguments are pairs](scripting/sb-vm.md#arguments-are-pairs).

**vmArgTagIsZero**: Reports whether an argument was left out (type byte 0, or index past the count). See [Native functions: Omitted arguments](scripting/natives.md#omitted-arguments).

**vmEntryGet / vmSubEntryGet**: Return the address of a global word of the script image, so natives can read and write script arrays. See [Native functions: Strings, arrays and data](scripting/natives.md#strings-arrays-and-data).

**Voice**: One sound effect: one note of one bank, named by a voice id. See [Audio: Voices](architecture/audio.md#8-voices-sound-effects-as-midi-messages).

**Voice bank handle**: A sound driver bank number from 3 to 8 given to a unit's voice bank, with fixed sound memory for party units and a bump area for enemies. See [The battle engine: Voice bank handles](game/battle-engine.md#voice-bank-handles).

**Voice id**: 32-bit name of a voice: bank slot in bits 24 to 30, note in bits 0 to 6, MIDI channel in the low four bits of the note. See [Audio: Voices](architecture/audio.md#8-voices-sound-effects-as-midi-messages). See also [Sound files: From a note to a sound](formats/sound.md#from-a-note-to-a-sound).

**Voice line stream**: A spoken line of a party member played from `battle/` on stream slot 1 by `streamPlayByKey`. See [The battle engine: Battle sound helpers](game/battle-engine.md#battle-sound-helpers).

**Voice record**: One of 24 records (`resRecordTable`) that remember looping sounds of the field, with fixed volume and pan or with an emitter position. See [Audio: Looping sounds](architecture/audio.md#9-looping-sounds-and-positional-sound).

**Voiced line**: An ADX file of `sound\adx\battle\` played in slot 1 for a technique or a situation. See [Audio: Battle sound](architecture/audio.md#17-battle-sound).

**Volume curve**: `seVolumeCurve`, the 36-point table that turns a level 0 to 127 into the attenuation value of `ADXT_SetOutVol`. See [Audio: Volume](architecture/audio.md#12-volume).

**VolumeRamp**: A 4-byte record (step, pause_value, target, current) that moves a level by a step per frame. See [Audio: Volume](architecture/audio.md#12-volume).

**.vsf**: The sample file of a KORG bank. See [Audio: Sound banks](architecture/audio.md#6-sound-banks).

**VSF**: The sample bank file of the KORG driver, magic `VSFp`: a 24-byte header and a chain of 40-byte sample records, each followed by its SPU2 ADPCM data. See [Sound files: VSF sample banks](formats/sound.md#vsf-sample-banks).

**VSync callback**: `xglRenderVSyncCallback`, run per field in interrupt context. Counts fields, records a busy drawing path, calls `ADXM_ExecVint`. See [xgl: Frame pacing](libraries/xgl.md#frame-pacing).

**VSync interval**: `xglRenderVSyncInterval`, the number of fields per game frame (2 normally, 1 during movies, 3 in one title phase). See [xgl: Frame pacing](libraries/xgl.md#frame-pacing).

**VSyncCount, xglRenderVSyncInterval**: The vertical sync counter and the number of syncs per frame (2, or 1 during movies). See [Rendering: Frame pacing](architecture/rendering.md#frame-pacing).

**VU0**: The vector unit coupled to the EE, with 32 four-float registers, used by all math code. See [Math: The VU0 unit](libraries/math.md#the-vu0-unit-in-plain-terms).

**vu0 current matrix**: The current matrix in `vf26`-`vf29` used by the effect code. See [Math: The vu0 current matrix](libraries/math.md#the-vu0-current-matrix).

**VU0 macros**: The recurring macro-mode blocks of `include/vu0_macro.h`, one per library register convention. See [Math: VU0 macros](libraries/math.md#vu0-macros).

**VU1 families and programs**: seven microcode units in each game, all for models in Episode I and five model, one effect and one water in Episode II. See [From Episode I to Episode II: The VU1 programs](architecture/from-episode-i.md#the-vu1-programs).

## W

**Wait counter**: `StatBlock.unitValue`, computed by `calcWct` as `(valueLevel - valueBias) * 9 + 180` with status changes. A unit enters the round when it reaches 0. See [Battle: The wait counter](game/battle.md#the-wait-counter).

**Wait escape**: `\k` or `\s` in a text block text. The reveal stops there until Circle is pressed. See [Fonts and text: The layout function](libraries/fonts-and-text.md#the-layout-function).

**Wait object**: Fire type 0: an object without picture that lives 31 frames. See [Battle effects: Wait (type 0)](game/battle-effects.md#wait-type-0).

**Wait slot**: Records 1 to 8 of the resource table, each holding one `.esp` file. Party members use 0 to 2 and enemies 3 to 7. See [sef: Wait slots](libraries/sef.md#wait-slots).

**waitHit**: Byte `0x24` of the effect request, the low byte of the hit kind. Zero makes hit wait rows skip without firing. See [sdv: The hit signal](libraries/sdv.md#the-hit-signal).

**Waiting native**: A native that writes 1 to its status word so that the VM repeats the call on the next frame. See [Native functions: Natives that wait](scripting/natives.md#natives-that-wait).

**Wall attribute**: The rules of `CheckWallAttr` that close an edge between two triangles (class walls, elevator edges, the key filter). See [Maps and units: Walking across triangles](game/map-and-units.md#walking-across-triangles).

**WAT chunk**: Chunk with the tag `WAT ` at the end of every model. A stub in most, a water table (`TiWaterSub`, `TiWatSimItem`) in ten models. See [The WAT chunk](formats/models-and-textures.md#the-wat-chunk).

**Water item (`TiWatSimItem`)**: One rectangular water sheet: grid, two height buffers, mask, normals. See [Animation: Water tables](libraries/animation.md#water-tables).

**Water program**: The VU1 program at `xglWaterPacketEntries` that draws water sheets in a surface pass and a reflection pass. See [pxy: The water program](libraries/pxy.md#the-water-program).

**Water reflection pass**: Draw mode 1, drawn into the work buffer and shrunk to half size for the water code. See [pxy: One camera](libraries/pxy.md#one-camera).

**Water table (`TiWaterSub`, "WAT ")**: A block inside a `.pxy` model that lists water sheets. See [Animation: Water tables](libraries/animation.md#water-tables).

**Wave**: Fire type 7: a spindle of ten golden arcs between two points. See [Battle effects: Wave (type 7)](game/battle-effects.md#wave-type-7).

**Way-home graph (`Homing_Add`, `Homing_Search`)**: Up to 64 linked nodes per character that lead back to node 0, the home. Used after a chase or a stun. See [Field enemies: The way home](game/field-enemies.md#the-way-home).

**White shake**: `gifStateStartWhiteShake`, the radial blur started by the script native `setLastDan` together with an actor sweep. See [Screen effects: Mode 0: radial blur](libraries/screen-effects.md#mode-0-radial-blur).

**Wide ASCII**: Style in which an ASCII byte is converted through `asciiEucTable` to a full-width EUC code and drawn from the large glyph cells. Set by code 25 bit 1 or `$han1,`. See [Fonts and text: The ASCII to EUC table](libraries/fonts-and-text.md#the-ascii-to-euc-table).

**Width table**: (`proportional_widths`): 384 two-byte entries, first and last inked column of each glyph. The advance is ink width plus 2. See [Text and fonts: The width table](formats/text-and-fonts.md#the-width-table).

**Wind block**: A node block of twelve curves in a `.dev` file that animates one directional or point wind source. See [Skeletons and motion: Wind blocks](formats/skeletons-and-motion.md#wind-blocks).

**Wind sequence**: The sequence that drives one directional or point wind source of the `Ti` library for the life of an effect. See [sdv: The wind sequence](libraries/sdv.md#the-wind-sequence).

**Wind slot**: One of four `TiWindSlot` entries that bind a wind source to the owner key and group key of an effect record. See [sdv: Wind slots](libraries/sdv.md#wind-slots).

**Window list / plain list**: Lists of held ids of box 1 and box 2 that the item screens show. See [Party and growth: Who can use what](game/party-and-growth.md#who-can-use-what).

**Windows-1252 half**: glyphs 256 to 383, reached by single bytes 0x80 to 0xFF, never used by shipped text. See [Text and fonts: Single bytes from 0x80](formats/text-and-fonts.md#single-bytes-from-0x80).

**Word offset**: An offset inside a script counted in 16-bit words from the script start. 0 and 0xFFFF mean absent. See [Effect data files: the effect script table](formats/effects.md#the-effect-script-table).

**Work area**: The 1 MiB of EE memory from `0x00B00000` to `0x00C00000`, handed out by the bump pointer `WorkEnd`. See [Memory: The work area and WorkEnd](architecture/memory.md#the-work-area-and-workend).

**Work buffer**: The third full-screen colour buffer at page `0x150` (`vram.work_buffer_base`, global `sRenderFrameBase`), used as render target and texture for reflections, glow, captures and menu backgrounds. See [Rendering: Video memory](architecture/rendering.md#video-memory).

**Work page**: The third frame page (`sRenderFrameBase`, page `0x150`), used for copies, effects and movie frames. See [xgl: Frame copies](libraries/xgl.md#frame-copies). See also [Screen effects: Pages and coordinates](libraries/screen-effects.md#pages-and-coordinates).

**Work range**: The range recorded by a second `GameResourceWorkAlloc` call while an arena exists (`gameResWorkStart`/`gameResWorkEnd`). See [Memory: Borrowing the resource area](architecture/memory.md#borrowing-the-resource-area).

**WorkEnd**: Global pointer to the first free byte of the work area. Memory is released by writing a saved value back. See [Memory: The work area and WorkEnd](architecture/memory.md#the-work-area-and-workend). See also [xgl: The subsystems](libraries/xgl.md#the-subsystems).

## X

**XAC curve**: The keyframe curve format of the Ti library (`xac*` functions): a run of keys with a 16-bit type word and a 16-bit frame. See [Animation: Keys](libraries/animation.md#keys).

**`XAC_KEY_END`**: Bit 0x4000 of `typeFlags`, the last key of a curve. See [Skeletons and motion: Keys](formats/skeletons-and-motion.md#keys).

**`XAC_KEY_LINEAR`**: Bit 0x8000 of `typeFlags`. It makes the segment a straight line only when a plain type 1 key follows. See [Skeletons and motion: How a curve is evaluated](formats/skeletons-and-motion.md#how-a-curve-is-evaluated).

**`XAct_Rsrc_MDL` / `TEX` / `XHR` / `XAP`**: The resource indices of the `.pxy` model, `.txy` texture, `.xhr` hierarchy and `.xap` motion table (names from the game's assert texts). See [Actors: The resource set](libraries/actors.md#the-resource-set).

**XAP group**: An entry of `xglCdXapGroups`: a shared motion file name (`man`, `woman`, `soldm`, ...) and the model ids that use it. 22 groups. See [Files and resources: Character and object sets](architecture/files-and-resources.md#character-and-object-sets).

**`XenoActorAllocMatrices`**: Allocates the matrix buffers from the hierarchy and binds all resources to the motion package. Called again after a model change. See [Actors: The life of an actor](libraries/actors.md#the-life-of-an-actor).

**`XenoActorDisp`**: The per-frame draw pass over all root actors, followed by the ground shadow pass. See [Actors: Drawing](libraries/actors.md#drawing).

**`XenoActorDoit`**: The per-frame animation pass over all root actors. See [Actors: The update each frame](libraries/actors.md#the-update-each-frame).

**`XenoActorDrawMotion`**: Sets the model library state for one actor, submits its model and draws its children. See [Actors: Drawing](libraries/actors.md#drawing).

**`XenoActorHidePart` / `XenoActorShowPart`**: Hide or show a model geometry by name. See [Actors: Visibility](libraries/actors.md#visibility).

**`XenoActorPoolInitDefault`**: Full reset of the pool and heap 5, called by every scene that builds its own actors. See [Actors: The pool and the matrix heap](libraries/actors.md#the-pool-and-the-matrix-heap).

**`XenoActorUpdateTree`**: Updates one actor and, recursively, its children. See [Actors: The update each frame](libraries/actors.md#the-update-each-frame).

**xgl**: The engine's system library in the main executable (`src/main/xgl_*.c`): frame, packets, files, controllers, memory card, HDD, studios, cameras, lights, text. See [xgl](libraries/xgl.md).

**xglAtan2 / xglAtan2Angle**: The polynomial arctangent (-pi to pi) and the `atanf`-based one (0 to 2 pi). See [Math: Trigonometry](libraries/math.md#trigonometry).

**XglCamera**: One view (`0x600` bytes): screen block, travel block, view volume. See [xgl: Camera data](libraries/xgl.md#camera-data).

**xglCdReadFileAsync**: Read with a tag (kind, sub) whose completion callback tells the owner (font slot, background layer, menu model, U.M.N. image). See [Main menu: Asynchronous reads](game/main-menu.md#asynchronous-reads).

**`xglCdServiceIfPending`**: Runs one step of the active read when the library is not idle and returns 1, else returns 0. The usual wait is `while (xglCdServiceIfPending() != 0) {}`. See [Files and resources: One step of the service](architecture/files-and-resources.md#one-step-of-the-service).

**xglCdSifLoadModule**: Loads `cdrom0:\IOP\<NAME>.IRX,1` with an argument list. See [Runtime libraries: Start-up](architecture/runtime-libraries.md#start-up).

**`XglClock`**: A real-time clock reading (second to year). `xglClockDayTime2UInt` converts it to seconds since 2000. See [Files and resources: The real-time clock](architecture/files-and-resources.md#the-real-time-clock). See also [Save data: Dates and play time](architecture/save-data.md#dates-and-play-time).

**xglColorLerp**: Per-channel interpolation of two packed 32-bit colours. See [Math: Conversions and rounding](libraries/math.md#conversions-and-rounding).

**xglCur**: The current matrix in `vf27`-`vf30` used by the Ti animation library and `XenoActor`. See [Math: The xglCur current matrix](libraries/math.md#the-xglcur-current-matrix).

**`xglDebFile*`**: The host file functions, stubs in this build that print "Don't use ..." and return -1. See [Files and resources: The host file functions](architecture/files-and-resources.md#the-host-file-functions).

**xglFlagsGet / xglFlagsSet**: Read or write a field of 1 to 16 bits of the script flags. The 32-bit and 64-bit forms compute a zero mask and do nothing useful. See [Save data: The script flags](architecture/save-data.md#the-script-flags).

**xglFRand**: The VU0 `R` register generator, a float from 1 to 2. See [Math: Random numbers](libraries/math.md#random-numbers).

**xglHddCheckCore**: Hard disk status probe that always returns -1 in this release, which disables every hard disk function. See [Save data: The hard disk](architecture/save-data.md#the-hard-disk).

**xglHddInstall**: Installer: creates and formats a PFS partition and copies the `xenosaga.NN` archives into `pfs0:/xenosaga.00`, with a progress callback. See [Save data: The installer](architecture/save-data.md#the-installer).

**xglJpegDecode**: Baseline JPEG decoder that writes to video memory or to a 32-bit or RGB555 buffer, with optional crop and colour scaling. Uses VU0 for the inverse DCT and colour conversion. See [Save data: The decoder](architecture/save-data.md#the-decoder).

**xglJpegEncode**: Baseline JPEG encoder that reads pixels from video memory MCU by MCU. See [Save data: The encoder](architecture/save-data.md#the-encoder).

**xglMcBuildPath / xglMcSetFullPath**: Build a card path from a base, a two-digit save number and a selector for the file. See [Save data: Paths and files](architecture/save-data.md#paths-and-files).

**xglMcEasyLoad / xglMcEasySave**: Blocking whole-file card helpers, used only by the memory card test. See [Save data: The blocking helpers](architecture/save-data.md#the-blocking-helpers).

**xglMcMain**: The memory card state machine, run once per frame. Returns `0x00FF00FF` while busy, else one status halfword per slot. See [Save data: The state machine](architecture/save-data.md#the-state-machine).

**xglMcRequest**: Queues a memory card request (16-entry ring) and runs one step of `xglMcMain`. See [Save data: Requests](architecture/save-data.md#requests).

**xglMcSetEntryNumbers**: Writes the save number and the play time as full-width digits into the `icon.sys` title. See [Save data: The save title](architecture/save-data.md#the-save-title).

**`xglMenu`**: The nested plain-text menu tree of the developer test screens, 16 entries in `menutbl`, up to eight levels. See [UI windows: The debug menu tree](libraries/ui-windows.md#the-debug-menu-tree-xglmenu).

**XglPacket**: The 0x30-byte record of one packet, used through the `sceVif1Pk*` functions, with the extra fields `limit`, `cursor`, `buffer` and `bufferSize`. See [Rendering: Packets](architecture/rendering.md#packets).

**xglSleep**: The end-of-frame call of every game loop: render step, sound settings, pad, disc code, cameras. It does not switch threads. See [Rendering: The frame without a render thread](architecture/rendering.md#the-frame-without-a-render-thread). See also [xgl: Threads and the frame](libraries/xgl.md#threads-and-the-frame).

**`xglSleep()`**: One frame step: `xglRenderFrame()`, `xglCdControlThread()`, `xglStudioUpdate()`. The game has no threads, every screen is a loop around this call. See [Boot and title: From main() to the title loop](game/title-and-boot.md#1-from-main-to-the-title-loop).

**xglSRand**: The linear congruential generator of the main program (`seed * 1103515245 + 12345`), 0 to 32767. See [Math: Random numbers](libraries/math.md#random-numbers).

**Xrg**: The plain C quaternion and vector helpers of the animation library. See [Math: Xrg quaternions](libraries/math.md#xrg-quaternions).

**Xrg quaternion functions**: The library's own quaternion helpers (`XrgQuaternionSlerp` and others). See [Animation: Quaternions](libraries/animation.md#quaternions).

**XSS**: An interleaved stream container with a bit map sector every `0x4000` sectors that routes each sector to one of two rings. Supported by the code, never used. See [Files and resources: Streams](architecture/files-and-resources.md#streams).

**XTX texture sheet**: A `.xtx` file, magic `XTX\0`: list of image uploads for 2D screens, read by `xglPacketTextureTrans`. See [XTX texture sheets](formats/models-and-textures.md#xtx-texture-sheets).

## Y

**Yield (opcode 0x1D)**: Stops a thread for the rest of the frame. See [The SB virtual machine: Waiting](scripting/sb-vm.md#waiting).

**Your Saves**: `pfs1:/Your Saves` on `hdd0:__common`, where the hard disk save functions keep save folders with the memory card names. See [Save data: Saves on the disk](architecture/save-data.md#saves-on-the-disk).

**ysegf**: Code name of the segment address screen (`src/main/ysegf.c`, entry `ysegfRunMenu`). See [G.S. Campaign: The segment address screen](game/gs-campaign.md#the-segment-address-screen).

**`YsegfSceneState`**: State record of the segment screen: `start`, `draw`, `finish`, `hook_end`. See [G.S. Campaign: Scene states](game/gs-campaign.md#scene-states).

## Z

**Z-write pre-pass**: Depth-only drawing of a translucent model before its color, done by `pxyDrawZwriteParts`. See [pxy: The order inside a pass](libraries/pxy.md#the-order-inside-a-pass).

**Zelda shadow**: Two small pointed shadows under two joints that point away from the light and fade when the joint lifts. See [Actors: Zelda shadow](libraries/actors.md#zelda-shadow).

**Zone**: A height bit 1, 2 or 4 (very likely A, B, C). Units, break sequences and techniques carry zone masks. See [Battle: Zones in the data](game/battle.md#zones-in-the-data).

**Zone code**: The three letters of a map name (`KML`, `MOM`, `SUI` ...) that name an area of the game. 20 zones have collision. See [Maps and field data: Naming](formats/maps-and-field-data.md#naming).
