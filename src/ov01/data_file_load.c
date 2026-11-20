#include "common.h"
#include "main/se_bank_load.h"
#include "main/game_data.h"
#include "main/xgl_cd_arena.h"
#include "ov01/data_file_load.h"
#include "ov01/data_unit_org_get.h"
#include "ov01/calc.h"
#include "ov01/unit_cmd.h"
#include "ov01/scene_script.h"
#include "ov01/handle_table.h"
#include "ov01/handle_state.h"
#include "main/xgl_cd.h"
#include "ov01/unit_tbl.h"
#include "ov01/data_record_get.h"
#include "main/xgl_flags.h"
#include "main/res_get_path.h"
#include "main/sef.h"

/*
 * The name strings of the data tables (model, motion, voice and map names, battle paths): the tables
 * themselves are in the overlay .data, which the TU map does not split per file yet, so they reach these strings
 * by name.
 */
const char battleDataFilePath[] = "yamamoto\\batdat.us";
const char battleStatTexturePath[] = "yamamoto\\stat\\stat.xtx";
const char faceAllMotionPath[] = "motion\\face_all.fap";
const char battleSeName[] = "bts_se";
const char thinkFilePath[] = "yamamoto\\think\\think";
const char thinkJpFilePath[] = "yamamoto\\think\\jp\\think";
const char modelDirPath[] = "model\\";
const char pxyExtensionName[] = ".pxy";
const char txyExtensionName[] = ".txy";
const char xhrExtensionName[] = ".xhr";
const char epfExtensionName[] = ".epf";
const char motionDirPath[] = "yamamoto\\mot\\";
const char xapExtensionName[] = ".xap";
const char battleDirPath[] = "battle/";
const char battleVoiceDirPath[] = "sound\\vsf\\battle\\";
const char vsfExtensionName[] = ".vsf";
const char prjExtensionName[] = ".prj";
const char mapDirPath[] = "yamamoto\\map\\";
const char roboPcRoboPcr05Br01Name[] = "robo\\pc_robo\\pcr05_br01";
const char roboPcRoboPcr05Sh01Name[] = "robo\\pc_robo\\pcr05_sh01";
const char roboPcRoboPcr05Mg01Name[] = "robo\\pc_robo\\pcr05_mg01";
const char charPcJrGun01bName[] = "char\\pc\\jr_gun01b";
const char charPcMmYm01Name[] = "char\\pc\\mm_ym01";
const char charPcJinSayaName[] = "char\\pc\\jin_saya";
const char charPcJinSw01Name[] = "char\\pc\\jin_sw01";
const char charPcShBox01Name[] = "char\\pc\\sh_box01";
const char charPcKosSw03Name[] = "char\\pc\\kos_sw03";
const char charPcKosSw01Name[] = "char\\pc\\kos_sw01";
const char charPcKosHw02Name[] = "char\\pc\\kos_hw02";
const char charPcKosHw01Name[] = "char\\pc\\kos_hw01";
const char gattai03Name[] = "gattai03";
const char gattai02Name[] = "gattai02";
const char gattai01Name[] = "gattai01";
const char db16Name[] = "db16";
const char db15Name[] = "db15";
const char db14Name[] = "db14";
const char db13Name[] = "db13";
const char db12Name[] = "db12";
const char db11Name[] = "db11";
const char db10Name[] = "db10";
const char db09Name[] = "db09";
const char db08Name[] = "db08";
const char db07Name[] = "db07";
const char db06Name[] = "db06";
const char db05Name[] = "db05";
const char db04Name[] = "db04";
const char db03Name[] = "db03";
const char db02Name[] = "db02";
const char db01Name[] = "db01";
const char pcr05Sp07Name[] = "pcr05_sp07";
const char pcr05Sp06Name[] = "pcr05_sp06";
const char pcr05Sp05Name[] = "pcr05_sp05";
const char pcr05Sp04Name[] = "pcr05_sp04";
const char pcr05Sp03Name[] = "pcr05_sp03";
const char pcr05Sp02Name[] = "pcr05_sp02";
const char pcr05Sp01Name[] = "pcr05_sp01";
const char pcr03Sp07Name[] = "pcr03_sp07";
const char pcr03Sp06Name[] = "pcr03_sp06";
const char pcr03Sp05Name[] = "pcr03_sp05";
const char pcr03Sp04Name[] = "pcr03_sp04";
const char pcr03Sp03Name[] = "pcr03_sp03";
const char pcr03Sp02Name[] = "pcr03_sp02";
const char pcr03Sp01Name[] = "pcr03_sp01";
const char pcr02Sp07Name[] = "pcr02_sp07";
const char pcr02Sp06Name[] = "pcr02_sp06";
const char pcr02Sp05Name[] = "pcr02_sp05";
const char pcr02Sp04Name[] = "pcr02_sp04";
const char pcr02Sp03Name[] = "pcr02_sp03";
const char pcr02Sp02Name[] = "pcr02_sp02";
const char pcr02Sp01Name[] = "pcr02_sp01";
const char kosmSp04Name[] = "kosm_sp04";
const char kosmSp03Name[] = "kosm_sp03";
const char kosmSp02Name[] = "kosm_sp02";
const char kosmSp01Name[] = "kosm_sp01";
const char jrGun02Name[] = "jr_gun02";
const char jrGun01Name[] = "jr_gun01";
const char mmYm01Name[] = "mm_ym01";
const char jinSayaName[] = "jin_saya";
const char jinSw01Name[] = "jin_sw01";
const char shBox01Name[] = "sh_box01";
const char bos031Name[] = "bos031";
const char bos030Name[] = "bos030";
const char bos029Name[] = "bos029";
const char bos028Name[] = "bos028";
const char bos027Name[] = "bos027";
const char bos026Name[] = "bos026";
const char bos025Name[] = "bos025";
const char bos024Name[] = "bos024";
const char bos023Name[] = "bos023";
const char bos022Name[] = "bos022";
const char bos021Name[] = "bos021";
const char bos020Name[] = "bos020";
const char bos019Name[] = "bos019";
const char bos018Name[] = "bos018";
const char bos017Name[] = "bos017";
const char bos016Name[] = "bos016";
const char bos015Name[] = "bos015";
const char bos014Name[] = "bos014";
const char bos013Name[] = "bos013";
const char bos012Name[] = "bos012";
const char bos011Name[] = "bos011";
const char bos010Name[] = "bos010";
const char bos009Name[] = "bos009";
const char bos008Name[] = "bos008";
const char bos007Name[] = "bos007";
const char bos006Name[] = "bos006";
const char bos005Name[] = "bos005";
const char bos004Name[] = "bos004";
const char bos003Name[] = "bos003";
const char bos002Name[] = "bos002";
const char bos001Name[] = "bos001";
const char mon004Name[] = "mon004";
const char mon003Name[] = "mon003";
const char mon002Name[] = "mon002";
const char mon001Name[] = "mon001";
const char vma003Name[] = "vma003";
const char vma002Name[] = "vma002";
const char vma001Name[] = "vma001";
const char uso012Name[] = "uso012";
const char uso011Name[] = "uso011";
const char uso010Name[] = "uso010";
const char uso009Name[] = "uso009";
const char uso008Name[] = "uso008";
const char uso007Name[] = "uso007";
const char uso006Name[] = "uso006";
const char uso005Name[] = "uso005";
const char uso004Name[] = "uso004";
const char uso003Name[] = "uso003";
const char uso002Name[] = "uso002";
const char uso001Name[] = "uso001";
const char uro015Name[] = "uro015";
const char uro014Name[] = "uro014";
const char uro013Name[] = "uro013";
const char uro012Name[] = "uro012";
const char uro011Name[] = "uro011";
const char uro010Name[] = "uro010";
const char uro009Name[] = "uro009";
const char uro008Name[] = "uro008";
const char uro007Name[] = "uro007";
const char uro006Name[] = "uro006";
const char uro005Name[] = "uro005";
const char uro004Name[] = "uro004";
const char uro003Name[] = "uro003";
const char uro002Name[] = "uro002";
const char uro001Name[] = "uro001";
const char uma013Name[] = "uma013";
const char uma012Name[] = "uma012";
const char uma011Name[] = "uma011";
const char uma010Name[] = "uma010";
const char uma009Name[] = "uma009";
const char uma008Name[] = "uma008";
const char uma007Name[] = "uma007";
const char uma006Name[] = "uma006";
const char uma005Name[] = "uma005";
const char uma004Name[] = "uma004";
const char uma003Name[] = "uma003";
const char uma002Name[] = "uma002";
const char uma001Name[] = "uma001";
const char cre018Name[] = "cre018";
const char cre017Name[] = "cre017";
const char cre016Name[] = "cre016";
const char cre015Name[] = "cre015";
const char cre014Name[] = "cre014";
const char cre013Name[] = "cre013";
const char cre012Name[] = "cre012";
const char cre011Name[] = "cre011";
const char cre010Name[] = "cre010";
const char cre009Name[] = "cre009";
const char cre008Name[] = "cre008";
const char cre007Name[] = "cre007";
const char cre006Name[] = "cre006";
const char cre005Name[] = "cre005";
const char cre004Name[] = "cre004";
const char cre003Name[] = "cre003";
const char cre002Name[] = "cre002";
const char cre001Name[] = "cre001";
const char gno019Name[] = "gno019";
const char gno018Name[] = "gno018";
const char gno017Name[] = "gno017";
const char gno016Name[] = "gno016";
const char gno015Name[] = "gno015";
const char gno014Name[] = "gno014";
const char gno013Name[] = "gno013";
const char gno012Name[] = "gno012";
const char gno011Name[] = "gno011";
const char gno010Name[] = "gno010";
const char gno009Name[] = "gno009";
const char gno008Name[] = "gno008";
const char gno007Name[] = "gno007";
const char gno006Name[] = "gno006";
const char gno005Name[] = "gno005";
const char gno004Name[] = "gno004";
const char gno003Name[] = "gno003";
const char gno002Name[] = "gno002";
const char gno001Name[] = "gno001";
const char pcr05Name[] = "pcr05";
const char pcr03Name[] = "pcr03";
const char pcr02Name[] = "pcr02";
const char jr00Name[] = "jr00";
const char momo00Name[] = "momo00";
const char zigg00Name[] = "zigg00";
const char jin00Name[] = "jin00";
const char shio00Name[] = "shio00";
const char kosm00Name[] = "kosm00";
const char chao00Name[] = "chao00";
const char bspB01Name[] = "BSP_B01";
const char batleBgName[] = "batle_bg";
const char midB05Name[] = "MID_B05";
const char e2Eve27Name[] = "E2_EVE27";
const char excB09Name[] = "EXC_B09";
const char excB08Name[] = "EXC_B08";
const char excB07Name[] = "EXC_B07";
const char ravB04Name[] = "RAV_B04";
const char midB06Name[] = "MID_B06";
const char exdB01Name[] = "EXD_B01";
const char e2Jsy06Name[] = "E2_JSY06";
const char omgB05Name[] = "OMG_B05";
const char e2Omg18Name[] = "E2_OMG18";
const char omgB06Name[] = "OMG_B06";
const char ravB05Name[] = "RAV_B05";
const char e2SuiB04Name[] = "E2_SUI_B04";
const char e2Olm15Name[] = "E2_OLM15";
const char e2Olm09Name[] = "E2_OLM09";
const char e2Olm05Name[] = "E2_OLM05";
const char e2Syd02Name[] = "E2_SYD02";
const char mom2B04Name[] = "MOM2_B04";
const char mom1B04Name[] = "MOM1_B04";
const char e2Mil15Name[] = "E2_MIL15";
const char e2Mid09Name[] = "E2_MID09";
const char e2Mid06Name[] = "E2_MID06";
const char e2KmlB04Name[] = "E2_KML_B04";
const char excB06Name[] = "EXC_B06";
const char excB05Name[] = "EXC_B05";
const char excB04Name[] = "EXC_B04";
const char excB03Name[] = "EXC_B03";
const char excB02Name[] = "EXC_B02";
const char excB01Name[] = "EXC_B01";
const char exbB01Name[] = "EXB_B01";
const char exaB02Name[] = "EXA_B02";
const char exaB01Name[] = "EXA_B01";
const char omgB04Name[] = "OMG_B04";
const char omgB03Name[] = "OMG_B03";
const char omgB02Name[] = "OMG_B02";
const char omgB01Name[] = "OMG_B01";
const char ravB03Name[] = "RAV_B03";
const char ravB02Name[] = "RAV_B02";
const char ravB01Name[] = "RAV_B01";
const char suiB03Name[] = "SUI_B03";
const char suiB02Name[] = "SUI_B02";
const char suiB01Name[] = "SUI_B01";
const char olmB05Name[] = "OLM_B05";
const char olmB04Name[] = "OLM_B04";
const char olmB03Name[] = "OLM_B03";
const char olmB02Name[] = "OLM_B02";
const char olmB01Name[] = "OLM_B01";
const char spaB01Name[] = "SPA_B01";
const char sydB03Name[] = "SYD_B03";
const char sydB02Name[] = "SYD_B02";
const char sydB01Name[] = "SYD_B01";
const char mom2B03Name[] = "MOM2_B03";
const char mom2B02Name[] = "MOM2_B02";
const char mom2B01Name[] = "MOM2_B01";
const char mom1B03Name[] = "MOM1_B03";
const char mom1B02Name[] = "MOM1_B02";
const char mom1B01Name[] = "MOM1_B01";
const char midB04Name[] = "MID_B04";
const char midB03Name[] = "MID_B03";
const char midB02Name[] = "MID_B02";
const char midB01Name[] = "MID_B01";
const char kmlB03Name[] = "KML_B03";
const char kmlB02Name[] = "KML_B02";
const char kmlB01Name[] = "KML_B01";
const char jrName[] = "jr";
const char momoName[] = "momo";
const char ziggyName[] = "ziggy";
const char jinName[] = "jin";
const char shionName[] = "shion";
const char kosmosName[] = "kosmos";
const char chaosName[] = "chaos";
const char eth110Name[] = "eth110";
const char eth109Name[] = "eth109";
const char eth108Name[] = "eth108";
const char eth107Name[] = "eth107";
const char eth106Name[] = "eth106";
const char eth105Name[] = "eth105";
const char eth104Name[] = "eth104";
const char eth103Name[] = "eth103";
const char eth102Name[] = "eth102";
const char eth101Name[] = "eth101";
const char eth100Name[] = "eth100";
const char eth099Name[] = "eth099";
const char eth098Name[] = "eth098";
const char eth097Name[] = "eth097";
const char eth096Name[] = "eth096";
const char eth095Name[] = "eth095";
const char eth094Name[] = "eth094";
const char eth093Name[] = "eth093";
const char eth092Name[] = "eth092";
const char eth091Name[] = "eth091";
const char eth090Name[] = "eth090";
const char eth089Name[] = "eth089";
const char eth088Name[] = "eth088";
const char eth087Name[] = "eth087";
const char eth086Name[] = "eth086";
const char eth085Name[] = "eth085";
const char eth084Name[] = "eth084";
const char eth083Name[] = "eth083";
const char eth082Name[] = "eth082";
const char eth081Name[] = "eth081";
const char eth080Name[] = "eth080";
const char eth079Name[] = "eth079";
const char eth078Name[] = "eth078";
const char eth077Name[] = "eth077";
const char eth076Name[] = "eth076";
const char eth075Name[] = "eth075";
const char eth074Name[] = "eth074";
const char eth073Name[] = "eth073";
const char eth072Name[] = "eth072";
const char eth071Name[] = "eth071";
const char eth070Name[] = "eth070";
const char eth069Name[] = "eth069";
const char eth068Name[] = "eth068";
const char eth067Name[] = "eth067";
const char eth066Name[] = "eth066";
const char eth065Name[] = "eth065";
const char eth064Name[] = "eth064";
const char eth063Name[] = "eth063";
const char eth062Name[] = "eth062";
const char eth061Name[] = "eth061";
const char eth060Name[] = "eth060";
const char eth059Name[] = "eth059";
const char eth058Name[] = "eth058";
const char eth057Name[] = "eth057";
const char eth056Name[] = "eth056";
const char eth055Name[] = "eth055";
const char eth054Name[] = "eth054";
const char eth053Name[] = "eth053";
const char eth052Name[] = "eth052";
const char eth051Name[] = "eth051";
const char eth050Name[] = "eth050";
const char eth049Name[] = "eth049";
const char eth048Name[] = "eth048";
const char eth047Name[] = "eth047";
const char eth046Name[] = "eth046";
const char eth045Name[] = "eth045";
const char eth044Name[] = "eth044";
const char eth043Name[] = "eth043";
const char eth042Name[] = "eth042";
const char eth041Name[] = "eth041";
const char eth040Name[] = "eth040";
const char eth039Name[] = "eth039";
const char eth038Name[] = "eth038";
const char eth037Name[] = "eth037";
const char eth036Name[] = "eth036";
const char eth035Name[] = "eth035";
const char eth034Name[] = "eth034";
const char eth033Name[] = "eth033";
const char eth032Name[] = "eth032";
const char eth031Name[] = "eth031";
const char eth030Name[] = "eth030";
const char eth029Name[] = "eth029";
const char eth028Name[] = "eth028";
const char eth027Name[] = "eth027";
const char eth026Name[] = "eth026";
const char eth025Name[] = "eth025";
const char eth024Name[] = "eth024";
const char eth023Name[] = "eth023";
const char eth022Name[] = "eth022";
const char eth021Name[] = "eth021";
const char eth020Name[] = "eth020";
const char eth019Name[] = "eth019";
const char eth018Name[] = "eth018";
const char eth017Name[] = "eth017";
const char eth016Name[] = "eth016";
const char eth015Name[] = "eth015";
const char eth014Name[] = "eth014";
const char eth013Name[] = "eth013";
const char eth012Name[] = "eth012";
const char eth011Name[] = "eth011";
const char eth010Name[] = "eth010";
const char eth009Name[] = "eth009";
const char eth008Name[] = "eth008";
const char eth007Name[] = "eth007";
const char eth006Name[] = "eth006";
const char eth005Name[] = "eth005";
const char eth004Name[] = "eth004";
const char eth003Name[] = "eth003";
const char eth002Name[] = "eth002";
const char eth001Name[] = "eth001";
const char itm40Name[] = "itm40";
const char itm39Name[] = "itm39";
const char itm38Name[] = "itm38";
const char itm37Name[] = "itm37";
const char itm36Name[] = "itm36";
const char itm35Name[] = "itm35";
const char itm34Name[] = "itm34";
const char itm33Name[] = "itm33";
const char itm32Name[] = "itm32";
const char itm31Name[] = "itm31";
const char itm30Name[] = "itm30";
const char itm29Name[] = "itm29";
const char itm28Name[] = "itm28";
const char itm27Name[] = "itm27";
const char itm26Name[] = "itm26";
const char itm25Name[] = "itm25";
const char itm24Name[] = "itm24";
const char itm23Name[] = "itm23";
const char itm22Name[] = "itm22";
const char itm21Name[] = "itm21";
const char itm20Name[] = "itm20";
const char itm19Name[] = "itm19";
const char itm18Name[] = "itm18";
const char itm17Name[] = "itm17";
const char itm16Name[] = "itm16";
const char itm15Name[] = "itm15";
const char itm14Name[] = "itm14";
const char itm13Name[] = "itm13";
const char itm12Name[] = "itm12";
const char itm11Name[] = "itm11";
const char itm10Name[] = "itm10";
const char itm09Name[] = "itm09";
const char itm08Name[] = "itm08";
const char itm07Name[] = "itm07";
const char itm06Name[] = "itm06";
const char itm05Name[] = "itm05";
const char itm04Name[] = "itm04";
const char itm03Name[] = "itm03";
const char itm02Name[] = "itm02";
const char itm01Name[] = "itm01";
const char kosSp04Name[] = "kos_sp04";
const char kosSp03Name[] = "kos_sp03";
const char kosSp02Name[] = "kos_sp02";
const char kosSp01Name[] = "kos_sp01";
const char pcr05Sp072Name[] = "pcr05_sp072";
const char pcr05Sp061Name[] = "pcr05_sp061";
const char pcr05Sp063Name[] = "pcr05_sp063";
const char pcr05Sp054Name[] = "pcr05_sp054";
const char pcr05Sp043Name[] = "pcr05_sp043";
const char pcr05Sp032Name[] = "pcr05_sp032";
const char pcr05Sp021Name[] = "pcr05_sp021";
const char pcr05Sp014Name[] = "pcr05_sp014";
const char pcr05Sp012Name[] = "pcr05_sp012";
const char pcr05Sp011Name[] = "pcr05_sp011";
const char pcr05Sp013Name[] = "pcr05_sp013";
const char pcr03Sp074Name[] = "pcr03_sp074";
const char pcr03Sp064Name[] = "pcr03_sp064";
const char pcr03Sp062Name[] = "pcr03_sp062";
const char pcr03Sp061Name[] = "pcr03_sp061";
const char pcr03Sp063Name[] = "pcr03_sp063";
const char pcr03Sp054Name[] = "pcr03_sp054";
const char pcr03Sp043Name[] = "pcr03_sp043";
const char pcr03Sp032Name[] = "pcr03_sp032";
const char pcr03Sp021Name[] = "pcr03_sp021";
const char pcr03Sp014Name[] = "pcr03_sp014";
const char pcr03Sp012Name[] = "pcr03_sp012";
const char pcr03Sp011Name[] = "pcr03_sp011";
const char pcr03Sp013Name[] = "pcr03_sp013";
const char pcr02Sp072Name[] = "pcr02_sp072";
const char pcr02Sp061Name[] = "pcr02_sp061";
const char pcr02Sp054Name[] = "pcr02_sp054";
const char pcr02Sp043Name[] = "pcr02_sp043";
const char pcr02Sp032Name[] = "pcr02_sp032";
const char pcr02Sp021Name[] = "pcr02_sp021";
const char pcr02Sp014Name[] = "pcr02_sp014";
const char pcr02Sp012Name[] = "pcr02_sp012";
const char pcr02Sp011Name[] = "pcr02_sp011";
const char pcr02Sp013Name[] = "pcr02_sp013";
const char jrRecName[] = "jr_rec";
const char jrDeadName[] = "jr_dead";
const char jrChgName[] = "jr_chg";
const char jrItm2Name[] = "jr_itm2";
const char jrItm1Name[] = "jr_itm1";
const char jrEth1Name[] = "jr_eth1";
const char momRecName[] = "mom_rec";
const char momDeadName[] = "mom_dead";
const char momChgName[] = "mom_chg";
const char momItm2Name[] = "mom_itm2";
const char momItm1Name[] = "mom_itm1";
const char momEth1Name[] = "mom_eth1";
const char zigRecName[] = "zig_rec";
const char zigDeadName[] = "zig_dead";
const char zigChgName[] = "zig_chg";
const char zigItm2Name[] = "zig_itm2";
const char zigItm1Name[] = "zig_itm1";
const char zigEth1Name[] = "zig_eth1";
const char jinRecName[] = "jin_rec";
const char jinDeadName[] = "jin_dead";
const char jinChgName[] = "jin_chg";
const char jinItm2Name[] = "jin_itm2";
const char jinItm1Name[] = "jin_itm1";
const char jinEth1Name[] = "jin_eth1";
const char sioRecName[] = "sio_rec";
const char sioDeadName[] = "sio_dead";
const char sioChgName[] = "sio_chg";
const char sioItm2Name[] = "sio_itm2";
const char sioItm1Name[] = "sio_itm1";
const char sioEth1Name[] = "sio_eth1";
const char kosRecName[] = "kos_rec";
const char kosDeadName[] = "kos_dead";
const char kosChgName[] = "kos_chg";
const char kosItm2Name[] = "kos_itm2";
const char kosItm1Name[] = "kos_itm1";
const char kosEth1Name[] = "kos_eth1";
const char choRecName[] = "cho_rec";
const char choDeadName[] = "cho_dead";
const char choChgName[] = "cho_chg";
const char choItm2Name[] = "cho_itm2";
const char choItm1Name[] = "cho_itm1";
const char choEth1Name[] = "cho_eth1";
const char jrBstName[] = "jr_bst";
const char momBstName[] = "mom_bst";
const char zigBstName[] = "zig_bst";
const char jinBstName[] = "jin_bst";
const char sioBstName[] = "sio_bst";
const char kosBstName[] = "kos_bst";
const char choBstName[] = "cho_bst";
const char jrBedName[] = "jr_bed";
const char momBedName[] = "mom_bed";
const char zigBedName[] = "zig_bed";
const char jinBedName[] = "jin_bed";
const char sioBedName[] = "sio_bed";
const char kosBedName[] = "kos_bed";
const char choBedName[] = "cho_bed";


/* Sizes and counts of the data loader (OV01 TU 8). */
#define DATA_FILE_BUF_SIZE 0x2E800
#define DATA_THINK_BUF_SIZE 0x12000
#define DATA_STAT_BUF_SIZE 0x41000
#define DATA_FACE_BUF_SIZE 0x8000
#define DATA_BATTLE_SE_PRJ_SIZE 0x800
#define DATA_BATTLE_SE_VSF_SIZE 0x1E800
#define DATA_ARENA_SIZE 0x11800
#define DATA_STREAM_SIZE 0x200000
#define DATA_STREAM_SMALL_SIZE 0x2A800
#define DATA_UNIT_HEAP_LARGE 0x12cc00
#define DATA_UNIT_HEAP_SIZE 0xc8800
#define DATA_UNIT_HEAP_CACHED_PART 0x57800
#define DATA_ID_COUNT 141
#define DATA_SLOT_COUNT 11
#define DATA_STREAM_COUNT 2
#define DATA_BATTLE_ID_COUNT 16
#define DATA_OTHER_ID_COUNT (DATA_ID_COUNT - DATA_BATTLE_ID_COUNT)
#define DATA_CD_SECTOR_SIZE 2048

void dataFileBufAlloc(void);
void *memset(void *, int, unsigned int);

static struct DataSlot dataSlotPool[DATA_SLOT_COUNT];
static struct DataStreamEntry dataStreamPool[DATA_STREAM_COUNT];
static struct DataEntry dataEntryBlock;
static struct DataFileRecord dataStatRecord;
static struct DataFileRecord dataFaceRecord;
static struct DataFileRecordPair dataBattleSeRecords;
static struct DataFileRecord dataThinkRecord;
static char dataPathBuf[256];
static char dataEntryPathBuf[256];

void dataFileInit(void) {
    memset(dataSlotPool, 0, sizeof(dataSlotPool));
    memset(dataStreamPool, 0, sizeof(dataStreamPool));
    memset(&dataEntryBlock, 0, sizeof(dataEntryBlock));
    dataCdSyncClear();
    ov01HeapInit();
    dataFileBufAlloc();
    dataEntryBlockReset();
}

extern int dataStatBuffer;

int dataStatBufGet(void)
{
    return dataStatBuffer;
}

extern int dataFaceHandle;

int dataFaceHandleGet(void)
{
    return dataFaceHandle;
}


void dataFileBufLoad(struct DataFileRecord *file_record, const char *path, int size) {
    char *buffer;

    buffer = ov01HeapAlloc(size);
    dataFileLoadNB(path, buffer);
    file_record->buffer = buffer;
    file_record->size = size;
}


int dataFileSlotLoad(struct DataFileRecord *file_record, char *path, char **next_buffer, int pass)
{
    int size;

    if (pass == 0) {
        if (path != 0) {
            file_record->size = (xglCdGetFileSize(path) + (DATA_CD_SECTOR_SIZE - 1)) / DATA_CD_SECTOR_SIZE * DATA_CD_SECTOR_SIZE;
            size = file_record->size;
        } else {
            file_record->size = 0;
            size = 0;
        }
        return size;
    }
    if (file_record->size > 0) {
        file_record->buffer = *next_buffer;
        dataFileLoadNB(path, file_record->buffer);
        *next_buffer += file_record->size;
    } else {
        file_record->buffer = 0;
    }
    return file_record->size;
}

extern const char *dataBatDatPath;

void dataBatDatLoad(void) {
    dataFileLoad(dataBatDatPath, &batDat);
}

int sprintf(char *, const char *, ...);
extern const char *dataThinkPath;
extern const char *dataThinkJpPath;


void dataThinkScriptLoad(int index)
{
    void *buffer;
    int size;

    sprintf(dataPathBuf, "%s%03d.sb", dataThinkPath, index);
    buffer = ov01HeapAlloc(DATA_THINK_BUF_SIZE);
    gameWork.resBuffer = buffer;
    dataFileLoad(dataPathBuf, ov01ScriptKeyGet());
    dataThinkRecord.buffer = buffer;
    dataThinkRecord.size = DATA_THINK_BUF_SIZE;
    size = (xglCdGetFileSize(dataPathBuf) + (DATA_CD_SECTOR_SIZE - 1)) / DATA_CD_SECTOR_SIZE * DATA_CD_SECTOR_SIZE;
    sprintf(dataPathBuf, "%s%03d.sbl", dataThinkJpPath, index);
    dataFileLoad(dataPathBuf, (char *)ov01ScriptKeyGet() + size);
    gameWork.imageEnd = (struct SbImageHeader *)((char *)ov01ScriptKeyGet() + size);
}

extern const char *dataStatPath;

void dataStatFileLoad(void) {
    dataFileBufLoad(&dataStatRecord, dataStatPath, DATA_STAT_BUF_SIZE);
}

extern const char *dataFacePath;

void dataFaceFileLoad(void) {
    int face_handle;

    face_handle = ov01LookupNonHeap(0x30041);
    if (face_handle != 0) {
        dataFaceHandle = face_handle;
        return;
    }
    dataFileBufLoad(&dataFaceRecord, dataFacePath, DATA_FACE_BUF_SIZE);
}

char *strcpy(char *, const char *);
char *strcat(char *, const char *);
extern const char *dataBattleSeName;
extern const char *dataVoiceDirPath;
extern const char *dataVsfExt;
extern const char *dataPrjExt;

void dataBattleSeLoad(void)
{
    strcpy(dataPathBuf, dataVoiceDirPath);
    strcat(dataPathBuf, dataBattleSeName);
    strcat(dataPathBuf, dataPrjExt);
    dataFileBufLoad(&dataBattleSeRecords.prj, dataPathBuf, DATA_BATTLE_SE_PRJ_SIZE);
    strcpy(dataPathBuf, dataVoiceDirPath);
    strcat(dataPathBuf, dataBattleSeName);
    strcat(dataPathBuf, dataVsfExt);
    dataFileBufLoad(&dataBattleSeRecords.vsf, dataPathBuf, DATA_BATTLE_SE_VSF_SIZE);
}


/* Uploads the battle SE files to sound bank 2, then frees their buffers. */
/* Uploads the battle sound effect bank (slot 2) to the sound driver, then frees its two EE buffers. */
void dataBattleSeUpload(void) {
    seBankLoadMemory(2, dataBattleSeRecords.vsf.buffer, dataBattleSeRecords.prj.buffer, 0);
    ov01HeapFree(dataBattleSeRecords.prj.buffer);
    ov01HeapFree(dataBattleSeRecords.vsf.buffer);
}

void dataCdArenaOpen(void) {
    void *arena_buffer;

    arena_buffer = ov01HeapAlloc(DATA_ARENA_SIZE);
    xglCdArenaOpen(arena_buffer, DATA_ARENA_SIZE);
}

extern const char *dataPxyExt;
extern const char *dataTxyExt;
extern const char *dataXhrExt;
extern const char *dataEpfExt;

int dataModelFilesLoad(struct DataSlot *table, const char *name, int index, int pass)
{
    int total;

    strcpy(dataPathBuf, name);
    strcat(dataPathBuf, dataEpfExt);
    total = dataFileSlotLoad(&table->models[index].epf, dataPathBuf, &table->head.next_buffer, pass);
    strcpy(dataPathBuf, name);
    strcat(dataPathBuf, dataXhrExt);
    total += dataFileSlotLoad(&table->models[index].xhr, dataPathBuf, &table->head.next_buffer, pass);
    strcpy(dataPathBuf, name);
    strcat(dataPathBuf, dataTxyExt);
    total += dataFileSlotLoad(&table->models[index].txy, dataPathBuf, &table->head.next_buffer, pass);
    strcpy(dataPathBuf, name);
    strcat(dataPathBuf, dataPxyExt);
    return total + dataFileSlotLoad(&table->models[index].pxy, dataPathBuf, &table->head.next_buffer, pass);
}

extern const char *dataMotDirPath;
extern const char *dataXapExt;

int dataMotFileLoad(struct DataSlot *table, const char *name, int index, int pass)
{
    struct DataSlot **table_ref = &table;
    strcpy(dataPathBuf, dataMotDirPath);
    strcat(dataPathBuf, name);
    strcat(dataPathBuf, dataXapExt);
    return dataFileSlotLoad(&(*table_ref)->motions[index].file, dataPathBuf, &(*table_ref)->head.next_buffer, pass);
}

int dataVoiceFilesLoad(struct DataSlot *slot, const char *name, int pass)
{
    char *voice_buffer;
    int prj_loaded;

    voice_buffer = 0;
    if (pass == 1) {
        voice_buffer = ov01HeapAlloc(slot->voice_files.prj.size + slot->voice_files.vsf.size);
    }
    strcpy(dataPathBuf, dataVoiceDirPath);
    strcat(dataPathBuf, name);
    strcat(dataPathBuf, dataPrjExt);
    prj_loaded = dataFileSlotLoad(&slot->voice_files.prj, dataPathBuf, &voice_buffer, pass);
    strcpy(dataPathBuf, dataVoiceDirPath);
    strcat(dataPathBuf, name);
    strcat(dataPathBuf, dataVsfExt);
    return prj_loaded + dataFileSlotLoad(&slot->voice_files.vsf, dataPathBuf, &voice_buffer, pass);
}

/* Uploads the slot's voice bank to the sound driver (seBankLoadMemory), marks its handle present and frees the .prj buffer. */

/* Uploads the slot's voice bank to the sound driver: the bank slot is the handle, the start address its
 * handle address. Then it sets the handle's state word and frees the PRJ buffer. */
void dataSlotVoiceUpload(struct DataSlot *slot) {
    int handle_address;

    handle_address = handleAddressGet(slot->handle);
    seBankLoadMemory(slot->handle, slot->voice_files.vsf.buffer, slot->voice_files.prj.buffer, handle_address);
    handleStateSet(slot->handle);
    ov01HeapFree(slot->voice_files.prj.buffer);
}

extern const char *dataMapDirPath;

int dataMapFileLoad(struct DataStreamEntry *table, const char *name, int index, int pass)
{
    struct DataMapFiles *files;
    int first_loaded;

    strcpy(dataPathBuf, dataMapDirPath);
    strcat(dataPathBuf, name);
    strcat(dataPathBuf, dataPxyExt);
    files = &table->maps[index];
    first_loaded = dataFileSlotLoad(&files->pxy, dataPathBuf, &table->head.next_buffer, pass);
    strcpy(dataPathBuf, dataMapDirPath);
    strcat(dataPathBuf, name);
    strcat(dataPathBuf, dataTxyExt);
    return first_loaded + dataFileSlotLoad(&files->txy, dataPathBuf, &table->head.next_buffer, pass);
}

int dataEntryFilesLoad(struct DataEntry *table, const char *name, int index, int pass)
{
    int total;

    strcpy(dataPathBuf, name);
    strcat(dataPathBuf, dataEpfExt);
    total = dataFileSlotLoad(&(&table->model)[index].epf, dataPathBuf, &table->head.next_buffer, pass);
    strcpy(dataPathBuf, name);
    strcat(dataPathBuf, dataXhrExt);
    total += dataFileSlotLoad(&(&table->model)[index].xhr, dataPathBuf, &table->head.next_buffer, pass);
    strcpy(dataPathBuf, name);
    strcat(dataPathBuf, dataTxyExt);
    total += dataFileSlotLoad(&(&table->model)[index].txy, dataPathBuf, &table->head.next_buffer, pass);
    strcpy(dataPathBuf, name);
    strcat(dataPathBuf, dataPxyExt);
    return total + dataFileSlotLoad(&(&table->model)[index].pxy, dataPathBuf, &table->head.next_buffer, pass);
}

void dataMotSlotLoad(struct DataEntry *table, const char *name, int index, int pass)
{
    struct DataEntry **table_ref = &table;
    strcpy(dataPathBuf, dataMotDirPath);
    strcat(dataPathBuf, name);
    strcat(dataPathBuf, dataXapExt);
    dataFileSlotLoad(&(*table_ref)->motions[index], dataPathBuf, &(*table_ref)->head.next_buffer, pass);
}

int dataEntryVoiceLoad(struct DataEntry *entry, const char *table_value, int which)
{
    return 0;
}

struct DataSlot *dataSlotFind(int id)
{
    int i;

    for (i = 0; i < DATA_SLOT_COUNT; i++) {
        if (dataSlotPool[i].id == id) {
            return &dataSlotPool[i];
        }
    }
    return 0;
}

struct DataSlot *dataUnitFileGet(int id) {
    struct DataSlot *slot;
    int first;
    int limit;
    int i;

    slot = dataSlotFind(id);
    if (slot != 0) {
        return slot;
    }
    if ((unsigned int)(id - DATA_BATTLE_ID_COUNT) >= DATA_OTHER_ID_COUNT) {
        first = 0;
        limit = 8;
    } else {
        first = 8;
        limit = DATA_SLOT_COUNT;
    }
    i = first;
    while (i < limit) {
        if (dataSlotPool[i].id == 0) {
            slot = &dataSlotPool[i];
            slot->id = id;
            slot->head.next_buffer = 0;
            slot->head.base.buffer = 0;
            break;
        }
        i++;
    }
    return slot;
}

int dataSlotRelease(int id) {
    struct DataSlot *slot;

    slot = dataSlotFind(id);
    if (slot == 0) {
        return 0;
    }
    memset(slot, 0, sizeof(*slot));
    slot->id = 0;
    return 1;
}

int dataSlotFlagSet(int id) {
    struct DataSlot *slot;
    Ov01HeapBlock *block;

    slot = dataSlotFind(id);
    if (slot == 0) {
        return 0;
    }
    block = ov01HeapBlockOf(slot->head.base.buffer);
    block->flags |= 2;
    return 1;
}

int dataSlotFlagClear(int id) {
    struct DataSlot *slot;
    Ov01HeapBlock *block;

    slot = dataSlotFind(id);
    if (slot == 0) {
        return 0;
    }
    block = ov01HeapBlockOf(slot->head.base.buffer);
    block->flags &= ~2;
    return 1;
}

int dataSlotReclaim(int min_size) {
    Ov01HeapBlock *block;
    int i;

    for (i = 0; i < DATA_SLOT_COUNT; i++) {
        if (dataSlotPool[i].id != 0 && dataSlotPool[i].head.base.buffer != 0) {
            block = ov01HeapBlockOf(dataSlotPool[i].head.base.buffer);
            if ((block->flags & 3) == 1) {
                block = ov01HeapBlockOf(dataSlotPool[i].head.base.buffer);
                if (block->size >= min_size) {
                    ov01HeapFree(dataSlotPool[i].head.base.buffer);
                    memset(&dataSlotPool[i], 0, sizeof(dataSlotPool[i]));
                    dataSlotPool[i].id = 0;
                    return 1;
                }
            }
        }
    }
    return 0;
}

struct DataSlot *dataUnitFileLoad(int id)
{
    struct DataSlot *slot;
    int index;
    int size;
    int heap_size;
    int file_id;
    int model_id;

    slot = dataUnitFileGet(id);
    if (slot == 0) {
        return 0;
    }
    if (slot->head.next_buffer == 0) {
        if ((unsigned int)(id - DATA_BATTLE_ID_COUNT) >= DATA_OTHER_ID_COUNT) {
            heap_size = ((unsigned int)(id - 11) < 5) ? DATA_UNIT_HEAP_LARGE : DATA_UNIT_HEAP_SIZE;
            file_id = unitFileIdGet(id);
            if (ov01LookupNonHeap(file_id) != 0) {
                heap_size -= DATA_UNIT_HEAP_CACHED_PART;
            }
            slot->head.next_buffer = ov01HeapAlloc(heap_size);
            slot->head.base.buffer = slot->head.next_buffer;
            if (slot->head.next_buffer == 0) {
                dataSlotReclaim(heap_size);
                slot->head.next_buffer = ov01HeapAlloc(heap_size);
                slot->head.base.buffer = slot->head.next_buffer;
            }
        }
    }
    if (id < DATA_BATTLE_ID_COUNT) {
        dataSlotFlagSet(id);
    }
    size = dataUnitFileIdLoad(slot, id, 0);
    size += dataMotSetLoad(slot, id, 0);
    index = 0;
    while (index < 3) {
        model_id = 0;
        if (id < DATA_BATTLE_ID_COUNT) {
            model_id = *(short *)((char *)battleSlotGet(id) + index * 2 + 116);
        }
        if (model_id != 0) {
            index++;
            size += dataModelLoad(slot, model_id, index);
            size += dataModelMotLoad(slot, model_id, index);
        } else {
            index++;
        }
    }
    dataVoiceSetLoad(slot, id);
    if ((((unsigned int)(id - 11) >= 130) && (size > DATA_UNIT_HEAP_SIZE)) || ((id < DATA_BATTLE_ID_COUNT) && (size > DATA_UNIT_HEAP_LARGE))) {
        for (;;) {
        }
    }
    return slot;
}


/* Table offsets are plain ints here (the original computed base + offset for every record), so the slot is added to
 * them as a byte pointer: the typed slot is the parameter, `table` its byte view (typed spellings fold the constants). */
int dataUnitFileIdLoad(struct DataSlot *slot, int key, int index)
{
    char *table;
    int file_id;
    int stride_offset;
    int pxy_offset;
    int txy_offset;
    int xhr_offset;
    int epf_offset;
    struct DataFileRecord *record;
    char *base;
    char **cached_files;
    int total;
    void *heap_block;

    table = (char *)slot;

    file_id = unitFileIdGet(key);
    stride_offset = index * 0x30;
    pxy_offset = stride_offset + 0x10;
    record = (struct DataFileRecord *)(table + pxy_offset);
    if (file_id == record->loaded_index) {
        return 0;
    }
    cached_files = (char **)ov01LookupNonHeap(file_id);
    if (cached_files != 0) {
        base = table + 4;
        txy_offset = stride_offset + 0x1C;
        xhr_offset = stride_offset + 0x28;
        epf_offset = stride_offset + 0x34;
        record->loaded_index = file_id;
        *(char **)(base + pxy_offset) = cached_files[0];
        *(char **)(base + txy_offset) = cached_files[1];
        *(char **)(base + xhr_offset) = cached_files[2];
        *(char **)(base + epf_offset) = cached_files[3];
        return 0;
    }
    xglCdBuildFileName(dataEntryPathBuf, file_id);
    record->loaded_index = file_id;
    total = dataModelFilesLoad(slot, dataEntryPathBuf, index, 0);
    if ((unsigned int)(key - DATA_BATTLE_ID_COUNT) < DATA_OTHER_ID_COUNT) {
        heap_block = ov01HeapAlloc(total);
        slot->head.next_buffer = heap_block;
        slot->head.base.buffer = heap_block;
    }
    return dataModelFilesLoad(slot, dataEntryPathBuf, index, 1);
}

const char *dataMotNameGet(int table_index);

/* The table is a DataSlot, but the record offset is an int added to its address (see dataUnitFileIdLoad). */
int dataMotSetLoad(struct DataSlot *slot, int id, int index)
{
    char *table;
    struct DataFileRecord *record;
    const char *name;
    int name_index;
    int offset;
    int total_size;
    void *heap_block;

    table = (char *)slot;

    if (id >= DATA_ID_COUNT) {
        return 0;
    }
    name_index = dataMotNameIndexGet(id);
    offset = index * 0x18 + 0xD0;
    record = (struct DataFileRecord *)(table + offset);
    if (name_index == record->loaded_index) {
        return 0;
    }
    name = dataMotNameGet(name_index);
    record->loaded_index = name_index;
    total_size = dataMotFileLoad(slot, name, index, 0);
    if ((unsigned int)(id - DATA_BATTLE_ID_COUNT) < DATA_OTHER_ID_COUNT) {
        heap_block = ov01HeapAlloc(total_size);
        slot->head.next_buffer = heap_block;
        slot->head.base.buffer = heap_block;
    }
    return dataMotFileLoad(slot, name, index, 1);
}

const char *dataVoiceNameGet(int index);

int dataVoiceSetLoad(struct DataSlot *slot, int index) {
    int voice;
    const char *name;

    if (index >= DATA_ID_COUNT) {
        return 0;
    }
    voice = dataVoiceNameIndexGet(index);
    if (voice == slot->voice_files.prj.loaded_index) {
        return 0;
    }
    name = dataVoiceNameGet(voice);
    slot->voice_files.prj.loaded_index = voice;
    slot->handle = handleAlloc(index >= DATA_BATTLE_ID_COUNT, dataVoiceFilesLoad(slot, name, 0));
    return dataVoiceFilesLoad(slot, name, 1);
}

void dataSlotVoiceReset(int id) {
    struct DataSlot *slot;

    slot = dataSlotFind(id);
    if (slot != 0) {
        memset(&slot->voice_files, 0, sizeof(slot->voice_files));
        slot->handle = 0;
    }
}

extern const char *dataModelDirPath;
const char *dataModelNameGet(int table_index);

/* Byte offsets into the slot: see dataUnitFileIdLoad. */
int dataModelLoad(struct DataSlot *slot, int id, int index)
{
    char *table;
    int stride_offset;
    int pxy_offset;
    struct DataFileRecord *record;
    char *base;
    char **cached_files;
    int txy_offset;
    int xhr_offset;
    int epf_offset;
    const char *name;

    table = (char *)slot;

    stride_offset = index * 0x30;
    pxy_offset = stride_offset + 0x10;
    record = (struct DataFileRecord *)(table + pxy_offset);
    if (id == record->loaded_index) {
        return 0;
    }
    cached_files = (char **)ov01LookupNonHeap(dataModelKeyGet(id));
    if (cached_files != 0) {
        base = table + 4;
        txy_offset = stride_offset + 0x1C;
        xhr_offset = stride_offset + 0x28;
        epf_offset = stride_offset + 0x34;
        record->loaded_index = id;
        *(char **)(base + pxy_offset) = cached_files[0];
        *(char **)(base + txy_offset) = cached_files[1];
        *(char **)(base + xhr_offset) = cached_files[2];
        *(char **)(base + epf_offset) = cached_files[3];
        return 0;
    }
    name = dataModelNameGet(id);
    if (name == 0) {
        return 0;
    }
    strcpy(dataEntryPathBuf, dataModelDirPath);
    strcat(dataEntryPathBuf, name);
    record->loaded_index = id;
    dataModelFilesLoad(slot, dataEntryPathBuf, index, 0);
    return dataModelFilesLoad(slot, dataEntryPathBuf, index, 1);
}

int dataModelMotLoad(struct DataSlot *slot, int model_id, int index)
{
    char *table;
    struct DataFileRecord *record;
    const char *name;
    int name_index;
    int offset;

    table = (char *)slot;

    offset = index * 0x18 + 0xD0;
    record = (struct DataFileRecord *)(table + offset);
    name_index = model_id + 143;
    if (name_index == record->loaded_index) {
        return 0;
    }
    name = dataMotNameGet(name_index);
    if (name == 0) {
        return 0;
    }
    record->loaded_index = name_index;
    dataMotFileLoad(slot, name, index, 0);
    return dataMotFileLoad(slot, name, index, 1);
}

struct SlotKeySource {
    unsigned char unmodeled_00[0x1E];
    short key;
};


int unitEffectFilesSetup(int key) {
    struct SlotKeySource *source;

    if (key >= DATA_ID_COUNT) {
        return 1;
    }
    if (key < DATA_BATTLE_ID_COUNT) {
        source = batUnitRecordGet(key);
        sefSetupPlayer(source->key);
        return 1;
    }
    source = batUnitRecordGet(key);
    sefSetupEnemy(source->key);
    return 1;
}

struct DataStreamEntry *dataStreamFind(int key)
{
    int i;

    for (i = 0; i < DATA_STREAM_COUNT; i++) {
        if (dataStreamPool[i].key == key) {
            return &dataStreamPool[i];
        }
    }
    return 0;
}

struct DataStreamEntry *dataStreamAcquire(key)
int key;

{
    struct DataStreamEntry *entry;
    int i;

    entry = dataStreamFind(key);
    if (entry != 0) {
        return entry;
    }
    for (i = 0; i < DATA_STREAM_COUNT; i++) {
        if (dataStreamPool[i].key == 0) {
            entry = &dataStreamPool[i];
            entry->key = key;
            entry->head.next_buffer = 0;
            entry->head.base.buffer = 0;
            break;
        }
    }
    return entry;
}

int dataStreamRelease(int key) {
    if (dataStreamFind(key) != 0) {
        return 0;
    }
    /* The original passes a zero base address to the clear of 0x24 bytes. */
    memset(0, 0, 0x24);
    return 1;
}

int dataMapSetLoad(struct DataStreamEntry *buffer, int mapNumber, int index);
const char *dataMapNameGet(int table_index);

struct DataStreamEntry *dataStreamOpen(int mapNumber, int small_buffer)
{
    struct DataStreamEntry *buffer;
    int buffer_size;

    /* The original passes no key here: the register still holds the map number (mapNumber). */
    buffer = dataStreamAcquire();
    if (buffer == 0) {
        return buffer;
    }
    buffer_size = DATA_STREAM_SIZE;
    if (small_buffer != 0) {
        buffer_size = DATA_STREAM_SMALL_SIZE;
    }
    if (buffer->head.next_buffer == 0) {
        buffer->head.next_buffer = ov01HeapAlloc(buffer_size);
        buffer->head.base.buffer = buffer->head.next_buffer;
    }
    if (buffer_size < dataMapSetLoad(buffer, mapNumber, 0)) {
        while (1) {
        }
    }
    return buffer;
}

int dataMapSetLoad(struct DataStreamEntry *buffer, int mapNumber, int index)
{
    struct DataMapFiles *maps;
    int *entry;
    const char *name;

    maps = buffer->maps;
    entry = &maps[index].pxy.loaded_index;
    if (mapNumber == *entry) {
        return 0;
    }
    name = dataMapNameGet(mapNumber);
    *entry = mapNumber;
    dataMapFileLoad(buffer, name, index, 0);
    return dataMapFileLoad(buffer, name, index, 1);
}

struct DataEntry *dataEntryBlockGet(void)
{
    return &dataEntryBlock;
}

extern void *dataFileBufBase;

void dataFileBufAlloc(void) {
    dataFileBufBase = ov01HeapAlloc(DATA_FILE_BUF_SIZE);
}

int dataEntryBlockReset(void) {
    int *block;
    int preserved;

    block = (int *)&dataEntryBlock;
    preserved = block[1];
    memset(block, 0, sizeof(dataEntryBlock));
    block[1] = preserved;
    block[0] = preserved;
}


struct DataEntry *dataModelEntryLoad(int table_index) {
    struct DataEntry *entry;
    const char *table_text;

    entry = dataEntryBlockGet();
    strcpy(dataEntryPathBuf, dataModelDirPath);
    table_text = dataModelNameGet(table_index);
    strcat(dataEntryPathBuf, table_text);
    entry->model.pxy.loaded_index = table_index;
    dataEntryFilesLoad(entry, dataEntryPathBuf, 0, 0);
    dataEntryFilesLoad(entry, dataEntryPathBuf, 0, 1);
    return entry;
}

struct DataEntry *dataMotEntryLoad(int table_index, int index, int suffix_index)
{
    char *suffix[2] = { "_a", "_b" };
    struct DataEntry *table;
    const char *name;
    int *slot;

    table = dataEntryBlockGet();
    name = dataMotNameGet(table_index);
    if (199 <= table_index && table_index < 215) {
        strcpy(dataEntryPathBuf, name);
        strcat(dataEntryPathBuf, suffix[suffix_index]);
        name = dataEntryPathBuf;
    }
    table->motions[index].loaded_index = table_index;
    dataMotSlotLoad(table, name, index, 0);
    dataMotSlotLoad(table, name, index, 1);
    return table;
}


struct DataEntry *dataVoiceEntryLoad(int table_index) {
    struct DataEntry *entry;
    const char *table_value;

    entry = dataEntryBlockGet();
    table_value = dataVoiceNameGet(table_index);
    entry->alt_index = table_index;
    dataEntryVoiceLoad(entry, table_value, 0);
    dataEntryVoiceLoad(entry, table_value, 1);
    return entry;
}

extern const char *dataModelNameTbl[];

const char *dataModelNameGet(int table_index) {
    const char *text;

    text = dataModelNameTbl[table_index];
    if (text != 0) {
        return text;
    }
    return "";
}

extern const char *dataMotNameTbl[];

const char *dataMotNameGet(int table_index) {
    const char *text;

    text = dataMotNameTbl[table_index];
    if (text != 0) {
        return text;
    }
    return "";
}

extern const char *dataVoiceNameTbl[];

const char *dataVoiceNameGet(int index)
{
    return dataVoiceNameTbl[index];
}

extern const char *dataMapNameTbl[];

const char *dataMapNameGet(int table_index) {
    const char *text;

    text = dataMapNameTbl[table_index];
    if (text != 0) {
        return text;
    }
    return "";
}

extern const char *dataBattleDirPath;
extern const char *battleCodePathNames[];
extern const char *battleSpecialPathNames[];

char *dataBattleCodePathGet(BattleUnit *object, int code)
{
    int kind;
    int sp_index;
    int flags;

    if ((unsigned int)(code - 44) < 27) {
        if ((unsigned int)(code - 44) < 7) {
            kind = code - 44;
        } else if ((unsigned int)(code - 54) < 7) {
            kind = code - 47;
        } else if ((unsigned int)(code - 64) < 7) {
            kind = code - 50;
        } else {
            return 0;
        }
        strcpy(dataPathBuf, dataBattleDirPath);
        sp_index = object->stats->linkedSlots[1] - 1;
        if (sp_index >= 0) {
            flags = xglFlagsGet(401, 8);
            if (flags < 10 && code == 64) {
                strcat(dataPathBuf, pcr05Sp01Name);
                return dataPathBuf;
            }
            if (flags < 10 && code == 67) {
                strcat(dataPathBuf, pcr05Sp04Name);
                return dataPathBuf;
            }
            strcat(dataPathBuf, battleSpecialPathNames[kind * 7 + sp_index]);
            return dataPathBuf;
        }
        return 0;
    }
    if ((unsigned int)(code - 71) < 10) {
        kind = code - 50;
    } else if ((unsigned int)(code - 81) < 16) {
        kind = code - 50;
    } else if ((unsigned int)(code - 631) < 40) {
        kind = code - 584;
    } else if (code >= 671) {
        kind = code - 584;
    } else {
        return 0;
    }
    strcpy(dataPathBuf, dataBattleDirPath);
    strcat(dataPathBuf, battleCodePathNames[kind]);
    return dataPathBuf;
}


extern const char *dataBattleActionNameTbl[];
char *dataBattleRandomPathGet(int kind, int mode);

char *dataBattleActionPathGet(int kind, int mode)
{
    const char *action_name;
    int mode_in_range;

    if ((unsigned int)(kind - 1) >= 10) {
        return 0;
    }
    mode_in_range = mode < 8;
    if (!mode_in_range) {
        return 0;
    }
    if ((unsigned int)(mode - 6) < 2) {
        action_name = dataBattleRandomPathGet(kind, mode);
    } else {
        action_name = dataBattleActionNameTbl[kind * 8 + mode];
    }
    strcpy(dataPathBuf, dataBattleDirPath);
    strcat(dataPathBuf, action_name);
    return dataPathBuf;
}

struct BattleNameSet {
    const char *dir;
    int code[11];
};

extern struct BattleNameSet dataBattleBstSets[];
extern struct BattleNameSet dataBattleBedSets[];
int battleCodeMatches(int kind, int code);

char *dataBattleRandomPathGet(int kind, int mode)
{
    struct BattleNameSet *name_set;
    int pick[11];
    char number_text[16];
    int i;
    int count;

    if (mode == 6) {
        name_set = dataBattleBstSets + (kind - 1);
    } else {
        name_set = dataBattleBedSets + (kind - 1);
    }
    count = 0;
    for (i = 10; i >= 0; i--) {
        if (battleCodeMatches(kind, name_set->code[i]) == 0) {
            continue;
        }
        if (name_set->code[i] >= 6) {
            pick[0] = i + 1;
            count = 1;
            break;
        }
        pick[count++] = i + 1;
    }
    count = pick[rnd(count - 1)];
    strcpy(dataEntryPathBuf, name_set->dir);
    sprintf(number_text, "%d", count);
    strcat(dataEntryPathBuf, number_text);
    return dataEntryPathBuf;
}

static inline BattleUnit *unitSlotAt(BattleUnit **table, int byteOffset)
{
    return *(BattleUnit **)(byteOffset + (int)table);
}

int battleCodeMatches(int kind, int code)
{
    BattleUnit **units;
    int count;
    int i;
    int hit;
    int slotOffset;
    int unitKind;

    switch (code) {
    case 0:
        return 0;
    case 1:
        return 1;
    case 2:
    case 3:
    case 4:
    case 5:
        count = unitTblGet(0, &units);
        for (i = 0; i < count; i++) {
            BattleUnit *unit = units[i];
            if (unit == 0) {
                continue;
            }
            unitKind = unit->stats->key;
            if (unitKind == 2 && code == 2) {
                return 1;
            }
            if (unitKind == 4 && code == 3) {
                return 1;
            }
            if (unitKind == 6 && code == 4) {
                return 1;
            }
            if (unitKind == 3 && code == 5) {
                return 1;
            }
        }
        return 0;
    case 6:
        count = unitTblGet(1, &units);
        for (i = 0; i < count; i++) {
            BattleUnit *unit = units[i];

            if (unit == 0) {
                continue;
            }
            if ((unit->stats->flags & 3) == 1) {
                return 1;
            }
        }
        return 0;
    case 7:
    case 8:
        count = unitTblGet(1, &units);
        for (i = 0; i < count; i++) {
            BattleUnit *unit = units[i];
            if (unit == 0) {
                continue;
            }
            unitKind = unit->stats->key;
            if ((unsigned int)(unitKind - 110) < 31) {
                if (code != 8) {
                    return 1;
                }
                if (ov01RollPercent(50) == 0) {
                    return 1;
                }
            }
        }
        return 0;
    case 9:
    case 10:
    case 11:
        count = unitTblGet(1, &units);
        for (i = 0; i < count; i++) {
            BattleUnit *unit = units[i];
            if (unit == 0) {
                continue;
            }
            unitKind = unit->stats->key;
            if (code == 9 && (unitKind == 110 || unitKind == 122)) {
                return 1;
            }
            if (code == 10 && unitKind == 126) {
                return 1;
            }
            if (code == 11 && (unitKind == 115 || unitKind == 127)) {
                return 1;
            }
        }
        return 0;
    case 12:
    case 13:
    case 14:
        count = unitTblGet(0, &units);
        i = 0;
        if (count > 0) {
            int sameKindCode = 12;
            int otherKindCode = 13;
            int fixedKindCode = 14;
            int fixedKind = 3;

            slotOffset = 0;
            do {
                if (unitSlotAt(units, slotOffset) != 0) {
                    if (code == sameKindCode && unitSlotAt(units, slotOffset)->stats->key == kind && partyUnitHpLow(unitSlotAt(units, slotOffset))) {
                        return 1;
                    }
                    if (code == otherKindCode && unitSlotAt(units, slotOffset)->stats->key != kind && partyUnitHpLow(unitSlotAt(units, slotOffset))) {
                        return 1;
                    }
                    if (code == fixedKindCode && unitSlotAt(units, slotOffset)->stats->key == fixedKind && partyUnitHpLow(unitSlotAt(units, slotOffset))) {
                        return 1;
                    }
                }
                i++;
                slotOffset += sizeof(BattleUnit *);
            } while (i < count);
        }
        return 0;
    case 15:
        count = unitTblGet(1, &units);
        hit = 0;
        for (i = 0; i < count; i++) {
            BattleUnit *unit = units[i];

            if (unit == 0) {
                continue;
            }
            if ((unit->stats->flags & 3) == 1) {
                hit = 1;
                break;
            }
        }
        if (hit == 0) {
            return 0;
        }
        count = unitTblGet(0, &units);
        for (i = 0; i < count; i++) {
            BattleUnit *unit = units[i];

            if (unit == 0) {
                continue;
            }
            if (unit->stats->key == 2) {
                return 1;
            }
        }
        return 0;
    case 16:
        count = unitTblGet(1, &units);
        hit = 0;
        for (i = 0; i < count; i++) {
            BattleUnit *unit = units[i];
            if (unit == 0) {
                continue;
            }
            unitKind = unit->stats->key;
            if ((unsigned int)(unitKind - 110) < 31) {
                hit = 1;
                break;
            }
        }
        if (hit == 0) {
            return 0;
        }
        count = unitTblGet(0, &units);
        for (i = 0; i < count; i++) {
            BattleUnit *unit = units[i];

            if (unit == 0) {
                continue;
            }
            if (unit->stats->key == 3) {
                return 1;
            }
        }
        return 0;
    case 17:
    case 18:
    case 19:
        if (code == 17 && ov01RollPercent(20) != 0) {
            return 1;
        }
        if (code == 18 && ov01RollPercent(15) != 0) {
            return 1;
        }
        if (code == 19 && ov01RollPercent(5) != 0) {
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

/* Initialized overlay data of this file (the scaffold pieces of .data, address order). */
const char *dataBatDatPath = battleDataFilePath;

const char *dataStatPath = battleStatTexturePath;

const char *dataFacePath = faceAllMotionPath;

const char *dataBattleSeName = battleSeName;

const char *dataThinkPath = thinkFilePath;

const char *dataThinkJpPath = thinkJpFilePath;

const char *dataModelDirPath = modelDirPath;

const char *dataPxyExt = pxyExtensionName;

const char *dataTxyExt = txyExtensionName;

const char *dataXhrExt = xhrExtensionName;

const char *dataEpfExt = epfExtensionName;

const char *dataMotDirPath = motionDirPath;

const char *dataXapExt = xapExtensionName;

const char *dataBattleDirPath = battleDirPath;

const char *dataVoiceDirPath = battleVoiceDirPath;

const char *dataVsfExt = vsfExtensionName;

const char *dataPrjExt = prjExtensionName;

const char *dataMapDirPath = mapDirPath;

const char *dataModelNameTbl[26] = {
    0, charPcKosHw01Name,
    charPcKosHw02Name, charPcKosSw01Name,
    0, charPcKosSw03Name,
    0, charPcShBox01Name,
    charPcJinSw01Name, charPcJinSayaName,
    charPcMmYm01Name, charPcJrGun01bName,
    charPcJrGun01bName, 0,
    0, 0,
    0, 0,
    0, roboPcRoboPcr05Mg01Name,
    roboPcRoboPcr05Sh01Name, roboPcRoboPcr05Br01Name,
    0, 0,
    0, 0,
};

const char *dataMotNameTbl[218] = {
    0, chao00Name,
    kosm00Name, shio00Name,
    jin00Name, zigg00Name,
    momo00Name, jr00Name,
    0, 0,
    0, pcr02Name,
    pcr03Name, pcr05Name,
    0, 0,
    gno001Name, gno002Name,
    gno003Name, gno004Name,
    gno005Name, gno006Name,
    gno007Name, gno008Name,
    gno009Name, gno010Name,
    gno011Name, gno012Name,
    gno013Name, gno014Name,
    gno015Name, gno016Name,
    gno017Name, gno018Name,
    gno019Name, cre001Name,
    cre002Name, cre003Name,
    cre004Name, cre005Name,
    cre006Name, cre007Name,
    cre008Name, cre009Name,
    cre010Name, cre011Name,
    cre012Name, cre013Name,
    cre014Name, cre015Name,
    cre016Name, cre017Name,
    cre018Name, uma001Name,
    uma002Name, uma003Name,
    uma004Name, uma005Name,
    uma006Name, uma007Name,
    uma008Name, uma009Name,
    uma010Name, uma011Name,
    uma012Name, uma013Name,
    uro001Name, uro002Name,
    uro003Name, uro004Name,
    uro005Name, uro006Name,
    uro007Name, uro008Name,
    uro009Name, uro010Name,
    uro011Name, uro012Name,
    uro013Name, uro014Name,
    uro015Name, uso001Name,
    uso002Name, uso003Name,
    uso004Name, uso005Name,
    uso006Name, uso007Name,
    uso008Name, uso009Name,
    uso010Name, uso011Name,
    uso012Name, vma001Name,
    vma002Name, vma003Name,
    uso001Name, uso002Name,
    uma002Name, cre005Name,
    uma005Name, uso005Name,
    uso006Name, uso007Name,
    uma006Name, uma012Name,
    mon001Name, mon002Name,
    mon003Name, mon004Name,
    bos001Name, bos002Name,
    bos003Name, bos004Name,
    bos005Name, bos006Name,
    bos007Name, bos008Name,
    bos009Name, bos010Name,
    bos011Name, bos012Name,
    bos013Name, bos014Name,
    bos015Name, bos016Name,
    bos017Name, bos018Name,
    bos019Name, bos020Name,
    bos021Name, bos022Name,
    bos023Name, bos024Name,
    bos025Name, bos026Name,
    bos027Name, bos028Name,
    bos029Name, bos030Name,
    bos031Name, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    shBox01Name, jinSw01Name,
    jinSayaName, mmYm01Name,
    jrGun01Name, jrGun02Name,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    kosmSp01Name, kosmSp02Name,
    kosmSp03Name, kosmSp04Name,
    0, 0,
    0, 0,
    0, 0,
    pcr02Sp01Name, pcr02Sp02Name,
    pcr02Sp03Name, pcr02Sp04Name,
    pcr02Sp05Name, pcr02Sp06Name,
    pcr02Sp07Name, pcr03Sp01Name,
    pcr03Sp02Name, pcr03Sp03Name,
    pcr03Sp04Name, pcr03Sp05Name,
    pcr03Sp06Name, pcr03Sp07Name,
    pcr05Sp01Name, pcr05Sp02Name,
    pcr05Sp03Name, pcr05Sp04Name,
    pcr05Sp05Name, pcr05Sp06Name,
    pcr05Sp07Name, db01Name,
    db02Name, db03Name,
    db04Name, db05Name,
    db06Name, db07Name,
    db08Name, db09Name,
    db10Name, db11Name,
    db12Name, db13Name,
    db14Name, db15Name,
    db16Name, gattai01Name,
    gattai02Name, gattai03Name,
};

const char *dataMapNameTbl[68] = {
    0, kmlB01Name,
    kmlB02Name, kmlB03Name,
    midB01Name, midB02Name,
    midB03Name, midB04Name,
    mom1B01Name, mom1B02Name,
    mom1B03Name, mom2B01Name,
    mom2B02Name, mom2B03Name,
    sydB01Name, sydB02Name,
    sydB03Name, spaB01Name,
    olmB01Name, olmB02Name,
    olmB03Name, olmB04Name,
    olmB05Name, suiB01Name,
    suiB02Name, suiB03Name,
    ravB01Name, ravB02Name,
    ravB03Name, omgB01Name,
    omgB02Name, omgB03Name,
    omgB04Name, exaB01Name,
    exaB02Name, exbB01Name,
    excB01Name, excB02Name,
    excB03Name, excB04Name,
    excB05Name, excB06Name,
    e2KmlB04Name, e2Mid06Name,
    e2Mid09Name, e2Mil15Name,
    mom1B04Name, mom2B04Name,
    e2Syd02Name, e2Olm05Name,
    e2Olm09Name, e2Olm15Name,
    e2SuiB04Name, ravB05Name,
    omgB06Name, e2Omg18Name,
    omgB05Name, e2Jsy06Name,
    exdB01Name, midB06Name,
    ravB04Name, excB07Name,
    excB08Name, excB09Name,
    e2Eve27Name, midB05Name,
    batleBgName, bspB01Name,
};

const char *dataVoiceNameTbl[142] = {
    0, chaosName,
    kosmosName, shionName,
    jinName, ziggyName,
    momoName, jrName,
    0, 0,
    0, pcr02Name,
    pcr03Name, pcr05Name,
    0, 0,
    gno001Name, gno002Name,
    gno003Name, gno004Name,
    gno005Name, gno006Name,
    gno007Name, gno008Name,
    gno009Name, gno010Name,
    gno011Name, gno012Name,
    gno013Name, gno014Name,
    gno015Name, gno016Name,
    gno017Name, gno018Name,
    gno019Name, cre001Name,
    cre002Name, cre003Name,
    cre004Name, cre005Name,
    cre006Name, cre007Name,
    cre008Name, cre009Name,
    cre010Name, cre011Name,
    cre012Name, cre013Name,
    cre014Name, cre015Name,
    cre016Name, cre017Name,
    cre018Name, uma001Name,
    uma002Name, uma003Name,
    uma004Name, uma005Name,
    uma006Name, uma007Name,
    uma008Name, uma009Name,
    uma010Name, uma011Name,
    uma012Name, uma013Name,
    uro001Name, uro002Name,
    uro003Name, uro004Name,
    uro005Name, uro006Name,
    uro007Name, uro008Name,
    uro009Name, uro010Name,
    uro011Name, uro012Name,
    uro013Name, uro014Name,
    uro015Name, uso001Name,
    uso002Name, uso003Name,
    uso004Name, uso005Name,
    uso006Name, uso007Name,
    uso008Name, uso009Name,
    uso010Name, uso011Name,
    uso012Name, vma001Name,
    vma002Name, vma003Name,
    uso001Name, uso002Name,
    uma002Name, cre005Name,
    uma005Name, uso005Name,
    uso006Name, uso007Name,
    uma006Name, uma012Name,
    mon001Name, mon002Name,
    mon003Name, mon004Name,
    bos001Name, bos002Name,
    bos003Name, bos004Name,
    bos005Name, bos006Name,
    bos007Name, bos008Name,
    bos009Name, bos010Name,
    bos011Name, bos012Name,
    bos013Name, bos014Name,
    bos015Name, bos016Name,
    bos017Name, bos018Name,
    bos019Name, bos020Name,
    bos021Name, bos022Name,
    bos023Name, bos024Name,
    bos025Name, bos026Name,
    bos027Name, bos028Name,
    bos029Name, bos030Name,
    bos031Name, 0,
};

const char *battleCodePathNames[198] = {
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, kosSp01Name,
    kosSp02Name, kosSp03Name,
    kosSp04Name, 0,
    0, 0,
    0, 0,
    0, db01Name,
    db02Name, db03Name,
    db04Name, db05Name,
    db06Name, db07Name,
    db08Name, db09Name,
    db10Name, db11Name,
    db12Name, db13Name,
    db14Name, db15Name,
    db16Name, itm01Name,
    itm02Name, itm03Name,
    itm04Name, itm05Name,
    itm06Name, itm07Name,
    itm08Name, itm09Name,
    itm10Name, itm11Name,
    itm12Name, itm13Name,
    itm14Name, itm15Name,
    itm16Name, itm17Name,
    itm18Name, itm19Name,
    itm20Name, itm21Name,
    itm22Name, itm23Name,
    itm24Name, itm25Name,
    itm26Name, itm27Name,
    itm28Name, itm29Name,
    itm30Name, itm31Name,
    itm32Name, itm33Name,
    itm34Name, itm35Name,
    itm36Name, itm37Name,
    itm38Name, itm39Name,
    itm40Name, eth001Name,
    eth002Name, eth003Name,
    eth004Name, eth005Name,
    eth006Name, eth007Name,
    eth008Name, eth009Name,
    eth010Name, eth011Name,
    eth012Name, eth013Name,
    eth014Name, eth015Name,
    eth016Name, eth017Name,
    eth018Name, eth019Name,
    eth020Name, eth021Name,
    eth022Name, eth023Name,
    eth024Name, eth025Name,
    eth026Name, eth027Name,
    eth028Name, eth029Name,
    eth030Name, eth031Name,
    eth032Name, eth033Name,
    eth034Name, eth035Name,
    eth036Name, eth037Name,
    eth038Name, eth039Name,
    eth040Name, eth041Name,
    eth042Name, eth043Name,
    eth044Name, eth045Name,
    eth046Name, eth047Name,
    eth048Name, eth049Name,
    eth050Name, eth051Name,
    eth052Name, eth053Name,
    eth054Name, eth055Name,
    eth056Name, eth057Name,
    eth058Name, eth059Name,
    eth060Name, eth061Name,
    eth062Name, eth063Name,
    eth064Name, eth065Name,
    eth066Name, eth067Name,
    eth068Name, eth069Name,
    eth070Name, eth071Name,
    eth072Name, eth073Name,
    eth074Name, eth075Name,
    eth076Name, eth077Name,
    eth078Name, eth079Name,
    eth080Name, eth081Name,
    eth082Name, eth083Name,
    eth084Name, eth085Name,
    eth086Name, eth087Name,
    eth088Name, eth089Name,
    eth090Name, eth091Name,
    eth092Name, eth093Name,
    eth094Name, eth095Name,
    eth096Name, eth097Name,
    eth098Name, eth099Name,
    eth100Name, eth101Name,
    eth102Name, eth103Name,
    eth104Name, eth105Name,
    eth106Name, eth107Name,
    eth108Name, eth109Name,
    eth110Name, 0,
};

const char *battleSpecialPathNames[140] = {
    pcr02Sp013Name, 0,
    pcr02Sp011Name, pcr02Sp012Name,
    pcr02Sp014Name, 0,
    0, 0,
    0, pcr02Sp021Name,
    0, 0,
    0, 0,
    0, 0,
    0, pcr02Sp032Name,
    0, 0,
    0, pcr02Sp043Name,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    pcr02Sp054Name, 0,
    0, 0,
    0, pcr02Sp061Name,
    0, 0,
    0, 0,
    0, 0,
    0, pcr02Sp072Name,
    0, 0,
    0, pcr03Sp013Name,
    0, pcr03Sp011Name,
    pcr03Sp012Name, pcr03Sp014Name,
    0, 0,
    0, 0,
    pcr03Sp021Name, 0,
    0, 0,
    0, 0,
    0, 0,
    pcr03Sp032Name, 0,
    0, 0,
    pcr03Sp043Name, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, pcr03Sp054Name,
    0, 0,
    pcr03Sp063Name, 0,
    pcr03Sp061Name, pcr03Sp062Name,
    pcr03Sp064Name, 0,
    0, 0,
    0, 0,
    0, pcr03Sp074Name,
    0, 0,
    pcr05Sp013Name, 0,
    pcr05Sp011Name, pcr05Sp012Name,
    pcr05Sp014Name, 0,
    0, 0,
    0, pcr05Sp021Name,
    0, 0,
    0, 0,
    0, 0,
    0, pcr05Sp032Name,
    0, 0,
    0, pcr05Sp043Name,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    pcr05Sp054Name, 0,
    0, pcr05Sp063Name,
    0, pcr05Sp061Name,
    0, 0,
    0, 0,
};

const char *dataBattleActionNameTbl[64] = {
    0, 0,
    0, pcr05Sp072Name,
    0, 0,
    0, 0,
    choEth1Name, choItm1Name,
    choItm2Name, choChgName,
    choDeadName, choRecName,
    0, 0,
    kosEth1Name, kosItm1Name,
    kosItm2Name, kosChgName,
    kosDeadName, kosRecName,
    0, 0,
    sioEth1Name, sioItm1Name,
    sioItm2Name, sioChgName,
    sioDeadName, sioRecName,
    0, 0,
    jinEth1Name, jinItm1Name,
    jinItm2Name, jinChgName,
    jinDeadName, jinRecName,
    0, 0,
    zigEth1Name, zigItm1Name,
    zigItm2Name, zigChgName,
    zigDeadName, zigRecName,
    0, 0,
    momEth1Name, momItm1Name,
    momItm2Name, momChgName,
    momDeadName, momRecName,
    0, 0,
    jrEth1Name, jrItm1Name,
    jrItm2Name, jrChgName,
    jrDeadName, jrRecName,
    0, 0,
};

struct BattleNameSet dataBattleBstSets[7] = {
    { choBstName, { 1, 1, 1, 1, 6, 0, 0, 0, 0, 0, 0 } },
    { kosBstName, { 1, 5, 16, 6, 0, 0, 0, 0, 0, 0, 0 } },
    { sioBstName, { 1, 1, 1, 1, 2, 3, 3, 3, 7, 8, 15 } },
    { jinBstName, { 1, 1, 1, 6, 7, 8, 9, 10, 0, 0, 0 } },
    { zigBstName, { 1, 1, 1, 7, 0, 0, 0, 0, 0, 0, 0 } },
    { momBstName, { 1, 1, 1, 7, 0, 0, 0, 0, 0, 0, 0 } },
    { jrBstName, { 1, 1, 4, 7, 11, 10, 0, 0, 0, 0, 0 } }
};

struct BattleNameSet dataBattleBedSets[7] = {
    { choBedName, { 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 } },
    { kosBedName, { 1, 17, 18, 19, 17, 17, 12, 0, 0, 0, 0 } },
    { sioBedName, { 1, 1, 1, 12, 13, 0, 0, 0, 0, 0, 0 } },
    { jinBedName, { 1, 1, 1, 1, 14, 0, 0, 0, 0, 0, 0 } },
    { zigBedName, { 1, 1, 12, 1, 0, 0, 0, 0, 0, 0, 0 } },
    { momBedName, { 1, 1, 1, 12, 0, 0, 0, 0, 0, 0, 0 } },
    { jrBedName, { 1, 1, 1, 1, 12, 0, 0, 0, 0, 0, 0 } }
};
