/*
 * OV01 original TU 8: 0x00a818e8..0x00a83d28 (53 functions)
 */
#include "common.h"
#include "main/tu021.h"
#include "main/tu048.h"
#include "main/tu092.h"
#include "ov01/tu008.h"
#include "ov01/tu009.h"
#include "ov01/tu023.h"
#include "ov01/tu040.h"
#include "ov01/tu069.h"
#include "ov01/tu070.h"

/*
 * The name strings of the data tables (model, motion, voice and map names, battle paths): the tables
 * themselves are in the overlay .data, which the TU map does not split per file yet, so they reach these strings
 * by name.
 */
const char D_00AB8CA0[] = "yamamoto\\batdat.us";
const char D_00AB8CB8[] = "yamamoto\\stat\\stat.xtx";
const char D_00AB8CD0[] = "motion\\face_all.fap";
const char D_00AB8CE8[] = "bts_se";
const char D_00AB8CF0[] = "yamamoto\\think\\think";
const char D_00AB8D08[] = "yamamoto\\think\\jp\\think";
const char D_00AB8D20[] = "model\\";
const char D_00AB8D28[] = ".pxy";
const char D_00AB8D30[] = ".txy";
const char D_00AB8D38[] = ".xhr";
const char D_00AB8D40[] = ".epf";
const char D_00AB8D48[] = "yamamoto\\mot\\";
const char D_00AB8D58[] = ".xap";
const char D_00AB8D60[] = "battle/";
const char D_00AB8D68[] = "sound\\vsf\\battle\\";
const char D_00AB8D80[] = ".vsf";
const char D_00AB8D88[] = ".prj";
const char D_00AB8D90[] = "yamamoto\\map\\";
const char D_00AB8DA0[] = "robo\\pc_robo\\pcr05_br01";
const char D_00AB8DB8[] = "robo\\pc_robo\\pcr05_sh01";
const char D_00AB8DD0[] = "robo\\pc_robo\\pcr05_mg01";
const char D_00AB8DE8[] = "char\\pc\\jr_gun01b";
const char D_00AB8E00[] = "char\\pc\\mm_ym01";
const char D_00AB8E10[] = "char\\pc\\jin_saya";
const char D_00AB8E28[] = "char\\pc\\jin_sw01";
const char D_00AB8E40[] = "char\\pc\\sh_box01";
const char D_00AB8E58[] = "char\\pc\\kos_sw03";
const char D_00AB8E70[] = "char\\pc\\kos_sw01";
const char D_00AB8E88[] = "char\\pc\\kos_hw02";
const char D_00AB8EA0[] = "char\\pc\\kos_hw01";
const char D_00AB8EB8[] = "gattai03";
const char D_00AB8EC8[] = "gattai02";
const char D_00AB8ED8[] = "gattai01";
const char D_00AB8EE8[] = "db16";
const char D_00AB8EF0[] = "db15";
const char D_00AB8EF8[] = "db14";
const char D_00AB8F00[] = "db13";
const char D_00AB8F08[] = "db12";
const char D_00AB8F10[] = "db11";
const char D_00AB8F18[] = "db10";
const char D_00AB8F20[] = "db09";
const char D_00AB8F28[] = "db08";
const char D_00AB8F30[] = "db07";
const char D_00AB8F38[] = "db06";
const char D_00AB8F40[] = "db05";
const char D_00AB8F48[] = "db04";
const char D_00AB8F50[] = "db03";
const char D_00AB8F58[] = "db02";
const char D_00AB8F60[] = "db01";
const char D_00AB8F68[] = "pcr05_sp07";
const char D_00AB8F78[] = "pcr05_sp06";
const char D_00AB8F88[] = "pcr05_sp05";
const char D_00AB8F98[] = "pcr05_sp04";
const char D_00AB8FA8[] = "pcr05_sp03";
const char D_00AB8FB8[] = "pcr05_sp02";
const char D_00AB8FC8[] = "pcr05_sp01";
const char D_00AB8FD8[] = "pcr03_sp07";
const char D_00AB8FE8[] = "pcr03_sp06";
const char D_00AB8FF8[] = "pcr03_sp05";
const char D_00AB9008[] = "pcr03_sp04";
const char D_00AB9018[] = "pcr03_sp03";
const char D_00AB9028[] = "pcr03_sp02";
const char D_00AB9038[] = "pcr03_sp01";
const char D_00AB9048[] = "pcr02_sp07";
const char D_00AB9058[] = "pcr02_sp06";
const char D_00AB9068[] = "pcr02_sp05";
const char D_00AB9078[] = "pcr02_sp04";
const char D_00AB9088[] = "pcr02_sp03";
const char D_00AB9098[] = "pcr02_sp02";
const char D_00AB90A8[] = "pcr02_sp01";
const char D_00AB90B8[] = "kosm_sp04";
const char D_00AB90C8[] = "kosm_sp03";
const char D_00AB90D8[] = "kosm_sp02";
const char D_00AB90E8[] = "kosm_sp01";
const char D_00AB90F8[] = "jr_gun02";
const char D_00AB9108[] = "jr_gun01";
const char D_00AB9118[] = "mm_ym01";
const char D_00AB9120[] = "jin_saya";
const char D_00AB9130[] = "jin_sw01";
const char D_00AB9140[] = "sh_box01";
const char D_00AB9150[] = "bos031";
const char D_00AB9158[] = "bos030";
const char D_00AB9160[] = "bos029";
const char D_00AB9168[] = "bos028";
const char D_00AB9170[] = "bos027";
const char D_00AB9178[] = "bos026";
const char D_00AB9180[] = "bos025";
const char D_00AB9188[] = "bos024";
const char D_00AB9190[] = "bos023";
const char D_00AB9198[] = "bos022";
const char D_00AB91A0[] = "bos021";
const char D_00AB91A8[] = "bos020";
const char D_00AB91B0[] = "bos019";
const char D_00AB91B8[] = "bos018";
const char D_00AB91C0[] = "bos017";
const char D_00AB91C8[] = "bos016";
const char D_00AB91D0[] = "bos015";
const char D_00AB91D8[] = "bos014";
const char D_00AB91E0[] = "bos013";
const char D_00AB91E8[] = "bos012";
const char D_00AB91F0[] = "bos011";
const char D_00AB91F8[] = "bos010";
const char D_00AB9200[] = "bos009";
const char D_00AB9208[] = "bos008";
const char D_00AB9210[] = "bos007";
const char D_00AB9218[] = "bos006";
const char D_00AB9220[] = "bos005";
const char D_00AB9228[] = "bos004";
const char D_00AB9230[] = "bos003";
const char D_00AB9238[] = "bos002";
const char D_00AB9240[] = "bos001";
const char D_00AB9248[] = "mon004";
const char D_00AB9250[] = "mon003";
const char D_00AB9258[] = "mon002";
const char D_00AB9260[] = "mon001";
const char D_00AB9268[] = "vma003";
const char D_00AB9270[] = "vma002";
const char D_00AB9278[] = "vma001";
const char D_00AB9280[] = "uso012";
const char D_00AB9288[] = "uso011";
const char D_00AB9290[] = "uso010";
const char D_00AB9298[] = "uso009";
const char D_00AB92A0[] = "uso008";
const char D_00AB92A8[] = "uso007";
const char D_00AB92B0[] = "uso006";
const char D_00AB92B8[] = "uso005";
const char D_00AB92C0[] = "uso004";
const char D_00AB92C8[] = "uso003";
const char D_00AB92D0[] = "uso002";
const char D_00AB92D8[] = "uso001";
const char D_00AB92E0[] = "uro015";
const char D_00AB92E8[] = "uro014";
const char D_00AB92F0[] = "uro013";
const char D_00AB92F8[] = "uro012";
const char D_00AB9300[] = "uro011";
const char D_00AB9308[] = "uro010";
const char D_00AB9310[] = "uro009";
const char D_00AB9318[] = "uro008";
const char D_00AB9320[] = "uro007";
const char D_00AB9328[] = "uro006";
const char D_00AB9330[] = "uro005";
const char D_00AB9338[] = "uro004";
const char D_00AB9340[] = "uro003";
const char D_00AB9348[] = "uro002";
const char D_00AB9350[] = "uro001";
const char D_00AB9358[] = "uma013";
const char D_00AB9360[] = "uma012";
const char D_00AB9368[] = "uma011";
const char D_00AB9370[] = "uma010";
const char D_00AB9378[] = "uma009";
const char D_00AB9380[] = "uma008";
const char D_00AB9388[] = "uma007";
const char D_00AB9390[] = "uma006";
const char D_00AB9398[] = "uma005";
const char D_00AB93A0[] = "uma004";
const char D_00AB93A8[] = "uma003";
const char D_00AB93B0[] = "uma002";
const char D_00AB93B8[] = "uma001";
const char D_00AB93C0[] = "cre018";
const char D_00AB93C8[] = "cre017";
const char D_00AB93D0[] = "cre016";
const char D_00AB93D8[] = "cre015";
const char D_00AB93E0[] = "cre014";
const char D_00AB93E8[] = "cre013";
const char D_00AB93F0[] = "cre012";
const char D_00AB93F8[] = "cre011";
const char D_00AB9400[] = "cre010";
const char D_00AB9408[] = "cre009";
const char D_00AB9410[] = "cre008";
const char D_00AB9418[] = "cre007";
const char D_00AB9420[] = "cre006";
const char D_00AB9428[] = "cre005";
const char D_00AB9430[] = "cre004";
const char D_00AB9438[] = "cre003";
const char D_00AB9440[] = "cre002";
const char D_00AB9448[] = "cre001";
const char D_00AB9450[] = "gno019";
const char D_00AB9458[] = "gno018";
const char D_00AB9460[] = "gno017";
const char D_00AB9468[] = "gno016";
const char D_00AB9470[] = "gno015";
const char D_00AB9478[] = "gno014";
const char D_00AB9480[] = "gno013";
const char D_00AB9488[] = "gno012";
const char D_00AB9490[] = "gno011";
const char D_00AB9498[] = "gno010";
const char D_00AB94A0[] = "gno009";
const char D_00AB94A8[] = "gno008";
const char D_00AB94B0[] = "gno007";
const char D_00AB94B8[] = "gno006";
const char D_00AB94C0[] = "gno005";
const char D_00AB94C8[] = "gno004";
const char D_00AB94D0[] = "gno003";
const char D_00AB94D8[] = "gno002";
const char D_00AB94E0[] = "gno001";
const char D_00AB94E8[] = "pcr05";
const char D_00AB94F0[] = "pcr03";
const char D_00AB94F8[] = "pcr02";
const char D_00AB9500[] = "jr00";
const char D_00AB9508[] = "momo00";
const char D_00AB9510[] = "zigg00";
const char D_00AB9518[] = "jin00";
const char D_00AB9520[] = "shio00";
const char D_00AB9528[] = "kosm00";
const char D_00AB9530[] = "chao00";
const char D_00AB9538[] = "BSP_B01";
const char D_00AB9540[] = "batle_bg";
const char D_00AB9550[] = "MID_B05";
const char D_00AB9558[] = "E2_EVE27";
const char D_00AB9568[] = "EXC_B09";
const char D_00AB9570[] = "EXC_B08";
const char D_00AB9578[] = "EXC_B07";
const char D_00AB9580[] = "RAV_B04";
const char D_00AB9588[] = "MID_B06";
const char D_00AB9590[] = "EXD_B01";
const char D_00AB9598[] = "E2_JSY06";
const char D_00AB95A8[] = "OMG_B05";
const char D_00AB95B0[] = "E2_OMG18";
const char D_00AB95C0[] = "OMG_B06";
const char D_00AB95C8[] = "RAV_B05";
const char D_00AB95D0[] = "E2_SUI_B04";
const char D_00AB95E0[] = "E2_OLM15";
const char D_00AB95F0[] = "E2_OLM09";
const char D_00AB9600[] = "E2_OLM05";
const char D_00AB9610[] = "E2_SYD02";
const char D_00AB9620[] = "MOM2_B04";
const char D_00AB9630[] = "MOM1_B04";
const char D_00AB9640[] = "E2_MIL15";
const char D_00AB9650[] = "E2_MID09";
const char D_00AB9660[] = "E2_MID06";
const char D_00AB9670[] = "E2_KML_B04";
const char D_00AB9680[] = "EXC_B06";
const char D_00AB9688[] = "EXC_B05";
const char D_00AB9690[] = "EXC_B04";
const char D_00AB9698[] = "EXC_B03";
const char D_00AB96A0[] = "EXC_B02";
const char D_00AB96A8[] = "EXC_B01";
const char D_00AB96B0[] = "EXB_B01";
const char D_00AB96B8[] = "EXA_B02";
const char D_00AB96C0[] = "EXA_B01";
const char D_00AB96C8[] = "OMG_B04";
const char D_00AB96D0[] = "OMG_B03";
const char D_00AB96D8[] = "OMG_B02";
const char D_00AB96E0[] = "OMG_B01";
const char D_00AB96E8[] = "RAV_B03";
const char D_00AB96F0[] = "RAV_B02";
const char D_00AB96F8[] = "RAV_B01";
const char D_00AB9700[] = "SUI_B03";
const char D_00AB9708[] = "SUI_B02";
const char D_00AB9710[] = "SUI_B01";
const char D_00AB9718[] = "OLM_B05";
const char D_00AB9720[] = "OLM_B04";
const char D_00AB9728[] = "OLM_B03";
const char D_00AB9730[] = "OLM_B02";
const char D_00AB9738[] = "OLM_B01";
const char D_00AB9740[] = "SPA_B01";
const char D_00AB9748[] = "SYD_B03";
const char D_00AB9750[] = "SYD_B02";
const char D_00AB9758[] = "SYD_B01";
const char D_00AB9760[] = "MOM2_B03";
const char D_00AB9770[] = "MOM2_B02";
const char D_00AB9780[] = "MOM2_B01";
const char D_00AB9790[] = "MOM1_B03";
const char D_00AB97A0[] = "MOM1_B02";
const char D_00AB97B0[] = "MOM1_B01";
const char D_00AB97C0[] = "MID_B04";
const char D_00AB97C8[] = "MID_B03";
const char D_00AB97D0[] = "MID_B02";
const char D_00AB97D8[] = "MID_B01";
const char D_00AB97E0[] = "KML_B03";
const char D_00AB97E8[] = "KML_B02";
const char D_00AB97F0[] = "KML_B01";
const char D_00AB97F8[] = "jr";
const char D_00AB9800[] = "momo";
const char D_00AB9808[] = "ziggy";
const char D_00AB9810[] = "jin";
const char D_00AB9818[] = "shion";
const char D_00AB9820[] = "kosmos";
const char D_00AB9828[] = "chaos";
const char D_00AB9830[] = "eth110";
const char D_00AB9838[] = "eth109";
const char D_00AB9840[] = "eth108";
const char D_00AB9848[] = "eth107";
const char D_00AB9850[] = "eth106";
const char D_00AB9858[] = "eth105";
const char D_00AB9860[] = "eth104";
const char D_00AB9868[] = "eth103";
const char D_00AB9870[] = "eth102";
const char D_00AB9878[] = "eth101";
const char D_00AB9880[] = "eth100";
const char D_00AB9888[] = "eth099";
const char D_00AB9890[] = "eth098";
const char D_00AB9898[] = "eth097";
const char D_00AB98A0[] = "eth096";
const char D_00AB98A8[] = "eth095";
const char D_00AB98B0[] = "eth094";
const char D_00AB98B8[] = "eth093";
const char D_00AB98C0[] = "eth092";
const char D_00AB98C8[] = "eth091";
const char D_00AB98D0[] = "eth090";
const char D_00AB98D8[] = "eth089";
const char D_00AB98E0[] = "eth088";
const char D_00AB98E8[] = "eth087";
const char D_00AB98F0[] = "eth086";
const char D_00AB98F8[] = "eth085";
const char D_00AB9900[] = "eth084";
const char D_00AB9908[] = "eth083";
const char D_00AB9910[] = "eth082";
const char D_00AB9918[] = "eth081";
const char D_00AB9920[] = "eth080";
const char D_00AB9928[] = "eth079";
const char D_00AB9930[] = "eth078";
const char D_00AB9938[] = "eth077";
const char D_00AB9940[] = "eth076";
const char D_00AB9948[] = "eth075";
const char D_00AB9950[] = "eth074";
const char D_00AB9958[] = "eth073";
const char D_00AB9960[] = "eth072";
const char D_00AB9968[] = "eth071";
const char D_00AB9970[] = "eth070";
const char D_00AB9978[] = "eth069";
const char D_00AB9980[] = "eth068";
const char D_00AB9988[] = "eth067";
const char D_00AB9990[] = "eth066";
const char D_00AB9998[] = "eth065";
const char D_00AB99A0[] = "eth064";
const char D_00AB99A8[] = "eth063";
const char D_00AB99B0[] = "eth062";
const char D_00AB99B8[] = "eth061";
const char D_00AB99C0[] = "eth060";
const char D_00AB99C8[] = "eth059";
const char D_00AB99D0[] = "eth058";
const char D_00AB99D8[] = "eth057";
const char D_00AB99E0[] = "eth056";
const char D_00AB99E8[] = "eth055";
const char D_00AB99F0[] = "eth054";
const char D_00AB99F8[] = "eth053";
const char D_00AB9A00[] = "eth052";
const char D_00AB9A08[] = "eth051";
const char D_00AB9A10[] = "eth050";
const char D_00AB9A18[] = "eth049";
const char D_00AB9A20[] = "eth048";
const char D_00AB9A28[] = "eth047";
const char D_00AB9A30[] = "eth046";
const char D_00AB9A38[] = "eth045";
const char D_00AB9A40[] = "eth044";
const char D_00AB9A48[] = "eth043";
const char D_00AB9A50[] = "eth042";
const char D_00AB9A58[] = "eth041";
const char D_00AB9A60[] = "eth040";
const char D_00AB9A68[] = "eth039";
const char D_00AB9A70[] = "eth038";
const char D_00AB9A78[] = "eth037";
const char D_00AB9A80[] = "eth036";
const char D_00AB9A88[] = "eth035";
const char D_00AB9A90[] = "eth034";
const char D_00AB9A98[] = "eth033";
const char D_00AB9AA0[] = "eth032";
const char D_00AB9AA8[] = "eth031";
const char D_00AB9AB0[] = "eth030";
const char D_00AB9AB8[] = "eth029";
const char D_00AB9AC0[] = "eth028";
const char D_00AB9AC8[] = "eth027";
const char D_00AB9AD0[] = "eth026";
const char D_00AB9AD8[] = "eth025";
const char D_00AB9AE0[] = "eth024";
const char D_00AB9AE8[] = "eth023";
const char D_00AB9AF0[] = "eth022";
const char D_00AB9AF8[] = "eth021";
const char D_00AB9B00[] = "eth020";
const char D_00AB9B08[] = "eth019";
const char D_00AB9B10[] = "eth018";
const char D_00AB9B18[] = "eth017";
const char D_00AB9B20[] = "eth016";
const char D_00AB9B28[] = "eth015";
const char D_00AB9B30[] = "eth014";
const char D_00AB9B38[] = "eth013";
const char D_00AB9B40[] = "eth012";
const char D_00AB9B48[] = "eth011";
const char D_00AB9B50[] = "eth010";
const char D_00AB9B58[] = "eth009";
const char D_00AB9B60[] = "eth008";
const char D_00AB9B68[] = "eth007";
const char D_00AB9B70[] = "eth006";
const char D_00AB9B78[] = "eth005";
const char D_00AB9B80[] = "eth004";
const char D_00AB9B88[] = "eth003";
const char D_00AB9B90[] = "eth002";
const char D_00AB9B98[] = "eth001";
const char D_00AB9BA0[] = "itm40";
const char D_00AB9BA8[] = "itm39";
const char D_00AB9BB0[] = "itm38";
const char D_00AB9BB8[] = "itm37";
const char D_00AB9BC0[] = "itm36";
const char D_00AB9BC8[] = "itm35";
const char D_00AB9BD0[] = "itm34";
const char D_00AB9BD8[] = "itm33";
const char D_00AB9BE0[] = "itm32";
const char D_00AB9BE8[] = "itm31";
const char D_00AB9BF0[] = "itm30";
const char D_00AB9BF8[] = "itm29";
const char D_00AB9C00[] = "itm28";
const char D_00AB9C08[] = "itm27";
const char D_00AB9C10[] = "itm26";
const char D_00AB9C18[] = "itm25";
const char D_00AB9C20[] = "itm24";
const char D_00AB9C28[] = "itm23";
const char D_00AB9C30[] = "itm22";
const char D_00AB9C38[] = "itm21";
const char D_00AB9C40[] = "itm20";
const char D_00AB9C48[] = "itm19";
const char D_00AB9C50[] = "itm18";
const char D_00AB9C58[] = "itm17";
const char D_00AB9C60[] = "itm16";
const char D_00AB9C68[] = "itm15";
const char D_00AB9C70[] = "itm14";
const char D_00AB9C78[] = "itm13";
const char D_00AB9C80[] = "itm12";
const char D_00AB9C88[] = "itm11";
const char D_00AB9C90[] = "itm10";
const char D_00AB9C98[] = "itm09";
const char D_00AB9CA0[] = "itm08";
const char D_00AB9CA8[] = "itm07";
const char D_00AB9CB0[] = "itm06";
const char D_00AB9CB8[] = "itm05";
const char D_00AB9CC0[] = "itm04";
const char D_00AB9CC8[] = "itm03";
const char D_00AB9CD0[] = "itm02";
const char D_00AB9CD8[] = "itm01";
const char D_00AB9CE0[] = "kos_sp04";
const char D_00AB9CF0[] = "kos_sp03";
const char D_00AB9D00[] = "kos_sp02";
const char D_00AB9D10[] = "kos_sp01";
const char D_00AB9D20[] = "pcr05_sp072";
const char D_00AB9D30[] = "pcr05_sp061";
const char D_00AB9D40[] = "pcr05_sp063";
const char D_00AB9D50[] = "pcr05_sp054";
const char D_00AB9D60[] = "pcr05_sp043";
const char D_00AB9D70[] = "pcr05_sp032";
const char D_00AB9D80[] = "pcr05_sp021";
const char D_00AB9D90[] = "pcr05_sp014";
const char D_00AB9DA0[] = "pcr05_sp012";
const char D_00AB9DB0[] = "pcr05_sp011";
const char D_00AB9DC0[] = "pcr05_sp013";
const char D_00AB9DD0[] = "pcr03_sp074";
const char D_00AB9DE0[] = "pcr03_sp064";
const char D_00AB9DF0[] = "pcr03_sp062";
const char D_00AB9E00[] = "pcr03_sp061";
const char D_00AB9E10[] = "pcr03_sp063";
const char D_00AB9E20[] = "pcr03_sp054";
const char D_00AB9E30[] = "pcr03_sp043";
const char D_00AB9E40[] = "pcr03_sp032";
const char D_00AB9E50[] = "pcr03_sp021";
const char D_00AB9E60[] = "pcr03_sp014";
const char D_00AB9E70[] = "pcr03_sp012";
const char D_00AB9E80[] = "pcr03_sp011";
const char D_00AB9E90[] = "pcr03_sp013";
const char D_00AB9EA0[] = "pcr02_sp072";
const char D_00AB9EB0[] = "pcr02_sp061";
const char D_00AB9EC0[] = "pcr02_sp054";
const char D_00AB9ED0[] = "pcr02_sp043";
const char D_00AB9EE0[] = "pcr02_sp032";
const char D_00AB9EF0[] = "pcr02_sp021";
const char D_00AB9F00[] = "pcr02_sp014";
const char D_00AB9F10[] = "pcr02_sp012";
const char D_00AB9F20[] = "pcr02_sp011";
const char D_00AB9F30[] = "pcr02_sp013";
const char D_00AB9F40[] = "jr_rec";
const char D_00AB9F48[] = "jr_dead";
const char D_00AB9F50[] = "jr_chg";
const char D_00AB9F58[] = "jr_itm2";
const char D_00AB9F60[] = "jr_itm1";
const char D_00AB9F68[] = "jr_eth1";
const char D_00AB9F70[] = "mom_rec";
const char D_00AB9F78[] = "mom_dead";
const char D_00AB9F88[] = "mom_chg";
const char D_00AB9F90[] = "mom_itm2";
const char D_00AB9FA0[] = "mom_itm1";
const char D_00AB9FB0[] = "mom_eth1";
const char D_00AB9FC0[] = "zig_rec";
const char D_00AB9FC8[] = "zig_dead";
const char D_00AB9FD8[] = "zig_chg";
const char D_00AB9FE0[] = "zig_itm2";
const char D_00AB9FF0[] = "zig_itm1";
const char D_00ABA000[] = "zig_eth1";
const char D_00ABA010[] = "jin_rec";
const char D_00ABA018[] = "jin_dead";
const char D_00ABA028[] = "jin_chg";
const char D_00ABA030[] = "jin_itm2";
const char D_00ABA040[] = "jin_itm1";
const char D_00ABA050[] = "jin_eth1";
const char D_00ABA060[] = "sio_rec";
const char D_00ABA068[] = "sio_dead";
const char D_00ABA078[] = "sio_chg";
const char D_00ABA080[] = "sio_itm2";
const char D_00ABA090[] = "sio_itm1";
const char D_00ABA0A0[] = "sio_eth1";
const char D_00ABA0B0[] = "kos_rec";
const char D_00ABA0B8[] = "kos_dead";
const char D_00ABA0C8[] = "kos_chg";
const char D_00ABA0D0[] = "kos_itm2";
const char D_00ABA0E0[] = "kos_itm1";
const char D_00ABA0F0[] = "kos_eth1";
const char D_00ABA100[] = "cho_rec";
const char D_00ABA108[] = "cho_dead";
const char D_00ABA118[] = "cho_chg";
const char D_00ABA120[] = "cho_itm2";
const char D_00ABA130[] = "cho_itm1";
const char D_00ABA140[] = "cho_eth1";
const char D_00ABA150[] = "jr_bst";
const char D_00ABA158[] = "mom_bst";
const char D_00ABA160[] = "zig_bst";
const char D_00ABA168[] = "jin_bst";
const char D_00ABA170[] = "sio_bst";
const char D_00ABA178[] = "kos_bst";
const char D_00ABA180[] = "cho_bst";
const char D_00ABA188[] = "jr_bed";
const char D_00ABA190[] = "mom_bed";
const char D_00ABA198[] = "zig_bed";
const char D_00ABA1A0[] = "jin_bed";
const char D_00ABA1A8[] = "sio_bed";
const char D_00ABA1B0[] = "kos_bed";
const char D_00ABA1B8[] = "cho_bed";


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
int dataEntryBlockReset(void);
void *memset(void *, int, unsigned int);

/* One map of a stream: the pxy and txy files (0x18 bytes). */
struct DataMapFiles {
    struct DataFileRecord pxy;
    struct DataFileRecord txy;
};

struct DataStreamEntry {
    struct DataFileHead head;
    int key;
    struct DataMapFiles maps[1];
};

static struct DataSlot dataSlotPool[DATA_SLOT_COUNT];
static struct DataStreamEntry dataStreamPool[DATA_STREAM_COUNT];
static struct DataEntry dataEntryBlock;
static struct DataFileRecord dataStatRecord;
static struct DataFileRecord dataFaceRecord;
static struct DataFileRecordPair dataBattleSeRecords;
static struct DataFileRecord D_00AC0E28;
static char dataPathBuf[256];
static char dataEntryPathBuf[256];

void dataFileInit(void) {
    memset(dataSlotPool, 0, sizeof(dataSlotPool));
    memset(dataStreamPool, 0, sizeof(dataStreamPool));
    memset(&dataEntryBlock, 0, sizeof(dataEntryBlock));
    ov01CdPendingReset();
    ov01HeapInit();
    dataFileBufAlloc();
    dataEntryBlockReset();
}

extern int D_00AC0DF4;

int dataStatBufGet(void)
{
    return D_00AC0DF4;
}

extern int dataFaceHandle;

int dataFaceHandleGet(void)
{
    return dataFaceHandle;
}


void dataFileBufLoad(struct DataFileRecord *file_record, char *path, int size) {
    char *buffer;

    buffer = ov01HeapAlloc(size);
    ov01CdReadFileAsync(path, buffer);
    file_record->buffer = buffer;
    file_record->size = size;
}

int xglCdGetFileSize(const char *path);

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
        ov01CdReadFileAsync(path, file_record->buffer);
        *next_buffer += file_record->size;
    } else {
        file_record->buffer = 0;
    }
    return file_record->size;
}

extern char *dataBatDatPath;

void dataBatDatLoad(void) {
    ov01CdReadFileSync(dataBatDatPath, &batDat);
}

int sprintf(char *, const char *, ...);
extern char *dataThinkPath;
extern char *dataThinkJpPath;


void dataThinkScriptLoad(int index)
{
    void *buffer;
    int size;

    sprintf(dataPathBuf, "%s%03d.sb", dataThinkPath, index);
    buffer = ov01HeapAlloc(DATA_THINK_BUF_SIZE);
    gameWork.resBuffer = buffer;
    ov01CdReadFileSync(dataPathBuf, ov01ScriptKeyGet());
    D_00AC0E28.buffer = buffer;
    D_00AC0E28.size = DATA_THINK_BUF_SIZE;
    size = (xglCdGetFileSize(dataPathBuf) + (DATA_CD_SECTOR_SIZE - 1)) / DATA_CD_SECTOR_SIZE * DATA_CD_SECTOR_SIZE;
    sprintf(dataPathBuf, "%s%03d.sbl", dataThinkJpPath, index);
    ov01CdReadFileSync(dataPathBuf, (char *)ov01ScriptKeyGet() + size);
    gameWork.imageEnd = (int)ov01ScriptKeyGet() + size;
}

extern char *dataStatPath;

void dataStatFileLoad(void) {
    dataFileBufLoad(&dataStatRecord, dataStatPath, DATA_STAT_BUF_SIZE);
}

extern char *dataFacePath;

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
extern char *dataBattleSeName;
extern char *dataVoiceDirPath;
extern char *dataVsfExt;
extern char *dataPrjExt;

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


void dataBattleSeRelease(void) {
    seBankLoadMemory(2, dataBattleSeRecords.vsf.buffer, dataBattleSeRecords.prj.buffer, 0);
    ov01HeapFree(dataBattleSeRecords.prj.buffer);
    ov01HeapFree(dataBattleSeRecords.vsf.buffer);
}

void dataCdArenaOpen(void) {
    int arena_buffer;

    arena_buffer = ov01HeapAlloc(DATA_ARENA_SIZE);
    xglCdArenaOpen(arena_buffer, DATA_ARENA_SIZE);
}

extern char *dataPxyExt;
extern char *dataTxyExt;
extern char *dataXhrExt;
extern char *dataEpfExt;

int dataUnitFilesLoad(struct DataSlot *table, char *name, int index, int pass)
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

extern char *dataMotDirPath;
extern char *dataXapExt;

int dataMotFileLoad(struct DataSlot *table, char *name, int index, int pass)
{
    struct DataSlot **table_ref = &table;
    strcpy(dataPathBuf, dataMotDirPath);
    strcat(dataPathBuf, name);
    strcat(dataPathBuf, dataXapExt);
    return dataFileSlotLoad(&(*table_ref)->motions[index].file, dataPathBuf, &(*table_ref)->head.next_buffer, pass);
}

int dataVoiceFilesLoad(struct DataSlot *slot, char *name, int pass)
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


void dataSlotUnload(struct DataSlot *slot) {
    int handle_address;

    handle_address = handleAddressGet(slot->handle);
    seBankLoadMemory(slot->handle, slot->voice_files.vsf.buffer, slot->voice_files.prj.buffer, handle_address);
    handleStateSet(slot->handle);
    ov01HeapFree(slot->voice_files.prj.buffer);
}

extern char *dataMapDirPath;

int dataMapFileLoad(struct DataStreamEntry *table, char *name, int index, int pass)
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

int dataEntryFilesLoad(struct DataEntry *table, char *name, int index, int pass)
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

void dataMotSlotLoad(struct DataEntry *table, char *name, int index, int pass)
{
    struct DataEntry **table_ref = &table;
    strcpy(dataPathBuf, dataMotDirPath);
    strcat(dataPathBuf, name);
    strcat(dataPathBuf, dataXapExt);
    dataFileSlotLoad(&(*table_ref)->motions[index], dataPathBuf, &(*table_ref)->head.next_buffer, pass);
}

int func_00A82368(struct DataEntry *entry, char *table_value, int which)
{
    return 0;
}

struct DataSlot *dataSlotFind(id)
int id;

{
    int i;

    for (i = 0; i < DATA_SLOT_COUNT; i++) {
        if (dataSlotPool[i].id == id) {
            return &dataSlotPool[i];
        }
    }
    return 0;
}

struct DataSlot *dataSlotAcquire(int id) {
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

struct DataSlot *dataUnitLoad(int id)
{
    struct DataSlot *slot;
    int index;
    int size;
    int heap_size;
    int file_id;
    int model_id;

    slot = dataSlotAcquire(id);
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

char *xglCdBuildFileName(char *buffer, unsigned int file_id);

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
    total = dataUnitFilesLoad(slot, dataEntryPathBuf, index, 0);
    if ((unsigned int)(key - DATA_BATTLE_ID_COUNT) < DATA_OTHER_ID_COUNT) {
        heap_block = ov01HeapAlloc(total);
        slot->head.next_buffer = heap_block;
        slot->head.base.buffer = heap_block;
    }
    return dataUnitFilesLoad(slot, dataEntryPathBuf, index, 1);
}

int func_00A9B298(int value);
unsigned char *dataMotNameGet(int table_index);

/* The table is a DataSlot, but the record offset is an int added to its address (see dataUnitFileIdLoad). */
int dataMotSetLoad(struct DataSlot *slot, int id, int index)
{
    char *table;
    struct DataFileRecord *record;
    unsigned char *name;
    int name_index;
    int offset;
    int total_size;
    void *heap_block;

    table = (char *)slot;

    if (id >= DATA_ID_COUNT) {
        return 0;
    }
    name_index = func_00A9B298(id);
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

int func_00A9B2A0(int index);
char *dataVoiceNameGet(int index);

int dataVoiceSetLoad(struct DataSlot *slot, int index) {
    int voice;
    char *name;

    if (index >= DATA_ID_COUNT) {
        return 0;
    }
    voice = func_00A9B2A0(index);
    if (voice == slot->voice_files.prj.loaded_index) {
        return 0;
    }
    name = dataVoiceNameGet(voice);
    slot->voice_files.prj.loaded_index = voice;
    slot->handle = handleAlloc(index >= DATA_BATTLE_ID_COUNT, dataVoiceFilesLoad(slot, name, 0));
    return dataVoiceFilesLoad(slot, name, 1);
}

void dataSlotReset(void) {
    struct DataSlot *slot;

    slot = dataSlotFind();
    if (slot != 0) {
        memset(&slot->voice_files, 0, sizeof(slot->voice_files));
        slot->handle = 0;
    }
}

extern char *dataModelDirPath;
unsigned char *dataModelNameGet(int table_index);
int dataModelKeyGet(int index);

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
    unsigned char *name;

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
    dataUnitFilesLoad(slot, dataEntryPathBuf, index, 0);
    return dataUnitFilesLoad(slot, dataEntryPathBuf, index, 1);
}

int dataModelMotLoad(struct DataSlot *slot, int model_id, int index)
{
    char *table;
    struct DataFileRecord *record;
    unsigned char *name;
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

void sefAcquireSlot(int key);
void sefAcquireHighSlot(int key);

int unitEffectSlotAcquire(int key) {
    struct SlotKeySource *source;

    if (key >= DATA_ID_COUNT) {
        return 1;
    }
    if (key < DATA_BATTLE_ID_COUNT) {
        source = batUnitRecordGet(key);
        sefAcquireSlot(source->key);
        return 1;
    }
    source = batUnitRecordGet(key);
    sefAcquireHighSlot(source->key);
    return 1;
}

struct DataStreamEntry *dataStreamFind(key)
int key;

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

int func_00A82F60(void) {
    if (dataStreamFind() != 0) {
        return 0;
    }
    /* The original passes a zero base address to the clear of 0x24 bytes. */
    memset(0, 0, 0x24);
    return 1;
}

int dataMapSetLoad(struct DataStreamEntry *buffer, int count, int index);
unsigned char *dataMapNameGet(int table_index);

struct DataStreamEntry *dataStreamOpen(int count, int small_buffer)
{
    struct DataStreamEntry *buffer;
    int buffer_size;

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
    if (buffer_size < dataMapSetLoad(buffer, count, 0)) {
        while (1) {
        }
    }
    return buffer;
}

int dataMapSetLoad(struct DataStreamEntry *buffer, int count, int index)
{
    struct DataMapFiles *maps;
    int *entry;
    char *name;

    maps = buffer->maps;
    entry = &maps[index].pxy.loaded_index;
    if (count == *entry) {
        return 0;
    }
    name = dataMapNameGet(count);
    *entry = count;
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
    unsigned char *table_text;

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
    char *name;
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
    char *table_value;

    entry = dataEntryBlockGet();
    table_value = dataVoiceNameGet(table_index);
    entry->alt_index = table_index;
    func_00A82368(entry, table_value, 0);
    func_00A82368(entry, table_value, 1);
    return entry;
}

extern unsigned char *dataModelNameTbl[];

unsigned char *dataModelNameGet(int table_index) {
    unsigned char *text;

    text = dataModelNameTbl[table_index];
    if (text != 0) {
        return text;
    }
    return "";
}

extern unsigned char *dataMotNameTbl[];

unsigned char *dataMotNameGet(int table_index) {
    unsigned char *text;

    text = dataMotNameTbl[table_index];
    if (text != 0) {
        return text;
    }
    return "";
}

extern char *dataVoiceNameTbl[];

char *dataVoiceNameGet(int index)
{
    return dataVoiceNameTbl[index];
}

extern unsigned char *dataMapNameTbl[];

unsigned char *dataMapNameGet(int table_index) {
    unsigned char *text;

    text = dataMapNameTbl[table_index];
    if (text != 0) {
        return text;
    }
    return "";
}

extern char *dataBattleDirPath;
extern char *D_00AB4C60[];
extern char *D_00AB4F78[];
int xglFlagsGet(int, int);

struct DataBattleState {
    unsigned char unmodeled_00[144];
    short one_based_index;
};

struct DataBattleOwner {
    unsigned char unmodeled_00[324];
    struct DataBattleState *state;
};

char *dataBattleCodePathGet(struct DataBattleOwner *owner, int code)
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
        sp_index = owner->state->one_based_index - 1;
        if (sp_index >= 0) {
            flags = xglFlagsGet(401, 8);
            if (flags < 10 && code == 64) {
                strcat(dataPathBuf, D_00AB8FC8);
                return dataPathBuf;
            }
            if (flags < 10 && code == 67) {
                strcat(dataPathBuf, D_00AB8F98);
                return dataPathBuf;
            }
            strcat(dataPathBuf, D_00AB4F78[kind * 7 + sp_index]);
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
    strcat(dataPathBuf, D_00AB4C60[kind]);
    return dataPathBuf;
}

int unitTblGet(int table, StatOwner ***units);
int ov01RollPercent(int percent);

extern char *dataBattleActionNameTbl[];
char *dataBattleRandomPathGet(int kind, int mode);

char *dataBattleActionPathGet(int kind, int mode)
{
    char *action_name;
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
    char *dir;
    int code[11];
};

extern struct BattleNameSet dataBattleBstSets[];
extern struct BattleNameSet dataBattleBedSets[];
int rnd(int);
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

static inline StatOwner *unitSlotAt(StatOwner **table, int byteOffset)
{
    return *(StatOwner **)(byteOffset + (int)table);
}

int battleCodeMatches(int kind, int code)
{
    StatOwner **units;
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
            StatOwner *unit = units[i];
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
            StatOwner *unit = units[i];

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
            StatOwner *unit = units[i];
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
            StatOwner *unit = units[i];
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
                slotOffset += sizeof(StatOwner *);
            } while (i < count);
        }
        return 0;
    case 15:
        count = unitTblGet(1, &units);
        hit = 0;
        for (i = 0; i < count; i++) {
            StatOwner *unit = units[i];

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
            StatOwner *unit = units[i];

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
            StatOwner *unit = units[i];
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
            StatOwner *unit = units[i];

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
char *dataBatDatPath = (char *) D_00AB8CA0;

char *dataStatPath = (char *) D_00AB8CB8;

char *dataFacePath = (char *) D_00AB8CD0;

char *dataBattleSeName = (char *) D_00AB8CE8;

char *dataThinkPath = (char *) D_00AB8CF0;

char *dataThinkJpPath = (char *) D_00AB8D08;

char *dataModelDirPath = (char *) D_00AB8D20;

char *dataPxyExt = (char *) D_00AB8D28;

char *dataTxyExt = (char *) D_00AB8D30;

char *dataXhrExt = (char *) D_00AB8D38;

char *dataEpfExt = (char *) D_00AB8D40;

char *dataMotDirPath = (char *) D_00AB8D48;

char *dataXapExt = (char *) D_00AB8D58;

char *dataBattleDirPath = (char *) D_00AB8D60;

char *dataVoiceDirPath = (char *) D_00AB8D68;

char *dataVsfExt = (char *) D_00AB8D80;

char *dataPrjExt = (char *) D_00AB8D88;

char *dataMapDirPath = (char *) D_00AB8D90;

unsigned char *dataModelNameTbl[26] = {
    0, (unsigned char *) D_00AB8EA0,
    (unsigned char *) D_00AB8E88, (unsigned char *) D_00AB8E70,
    0, (unsigned char *) D_00AB8E58,
    0, (unsigned char *) D_00AB8E40,
    (unsigned char *) D_00AB8E28, (unsigned char *) D_00AB8E10,
    (unsigned char *) D_00AB8E00, (unsigned char *) D_00AB8DE8,
    (unsigned char *) D_00AB8DE8, 0,
    0, 0,
    0, 0,
    0, (unsigned char *) D_00AB8DD0,
    (unsigned char *) D_00AB8DB8, (unsigned char *) D_00AB8DA0,
    0, 0,
    0, 0,
};

unsigned char *dataMotNameTbl[218] = {
    0, (unsigned char *) D_00AB9530,
    (unsigned char *) D_00AB9528, (unsigned char *) D_00AB9520,
    (unsigned char *) D_00AB9518, (unsigned char *) D_00AB9510,
    (unsigned char *) D_00AB9508, (unsigned char *) D_00AB9500,
    0, 0,
    0, (unsigned char *) D_00AB94F8,
    (unsigned char *) D_00AB94F0, (unsigned char *) D_00AB94E8,
    0, 0,
    (unsigned char *) D_00AB94E0, (unsigned char *) D_00AB94D8,
    (unsigned char *) D_00AB94D0, (unsigned char *) D_00AB94C8,
    (unsigned char *) D_00AB94C0, (unsigned char *) D_00AB94B8,
    (unsigned char *) D_00AB94B0, (unsigned char *) D_00AB94A8,
    (unsigned char *) D_00AB94A0, (unsigned char *) D_00AB9498,
    (unsigned char *) D_00AB9490, (unsigned char *) D_00AB9488,
    (unsigned char *) D_00AB9480, (unsigned char *) D_00AB9478,
    (unsigned char *) D_00AB9470, (unsigned char *) D_00AB9468,
    (unsigned char *) D_00AB9460, (unsigned char *) D_00AB9458,
    (unsigned char *) D_00AB9450, (unsigned char *) D_00AB9448,
    (unsigned char *) D_00AB9440, (unsigned char *) D_00AB9438,
    (unsigned char *) D_00AB9430, (unsigned char *) D_00AB9428,
    (unsigned char *) D_00AB9420, (unsigned char *) D_00AB9418,
    (unsigned char *) D_00AB9410, (unsigned char *) D_00AB9408,
    (unsigned char *) D_00AB9400, (unsigned char *) D_00AB93F8,
    (unsigned char *) D_00AB93F0, (unsigned char *) D_00AB93E8,
    (unsigned char *) D_00AB93E0, (unsigned char *) D_00AB93D8,
    (unsigned char *) D_00AB93D0, (unsigned char *) D_00AB93C8,
    (unsigned char *) D_00AB93C0, (unsigned char *) D_00AB93B8,
    (unsigned char *) D_00AB93B0, (unsigned char *) D_00AB93A8,
    (unsigned char *) D_00AB93A0, (unsigned char *) D_00AB9398,
    (unsigned char *) D_00AB9390, (unsigned char *) D_00AB9388,
    (unsigned char *) D_00AB9380, (unsigned char *) D_00AB9378,
    (unsigned char *) D_00AB9370, (unsigned char *) D_00AB9368,
    (unsigned char *) D_00AB9360, (unsigned char *) D_00AB9358,
    (unsigned char *) D_00AB9350, (unsigned char *) D_00AB9348,
    (unsigned char *) D_00AB9340, (unsigned char *) D_00AB9338,
    (unsigned char *) D_00AB9330, (unsigned char *) D_00AB9328,
    (unsigned char *) D_00AB9320, (unsigned char *) D_00AB9318,
    (unsigned char *) D_00AB9310, (unsigned char *) D_00AB9308,
    (unsigned char *) D_00AB9300, (unsigned char *) D_00AB92F8,
    (unsigned char *) D_00AB92F0, (unsigned char *) D_00AB92E8,
    (unsigned char *) D_00AB92E0, (unsigned char *) D_00AB92D8,
    (unsigned char *) D_00AB92D0, (unsigned char *) D_00AB92C8,
    (unsigned char *) D_00AB92C0, (unsigned char *) D_00AB92B8,
    (unsigned char *) D_00AB92B0, (unsigned char *) D_00AB92A8,
    (unsigned char *) D_00AB92A0, (unsigned char *) D_00AB9298,
    (unsigned char *) D_00AB9290, (unsigned char *) D_00AB9288,
    (unsigned char *) D_00AB9280, (unsigned char *) D_00AB9278,
    (unsigned char *) D_00AB9270, (unsigned char *) D_00AB9268,
    (unsigned char *) D_00AB92D8, (unsigned char *) D_00AB92D0,
    (unsigned char *) D_00AB93B0, (unsigned char *) D_00AB9428,
    (unsigned char *) D_00AB9398, (unsigned char *) D_00AB92B8,
    (unsigned char *) D_00AB92B0, (unsigned char *) D_00AB92A8,
    (unsigned char *) D_00AB9390, (unsigned char *) D_00AB9360,
    (unsigned char *) D_00AB9260, (unsigned char *) D_00AB9258,
    (unsigned char *) D_00AB9250, (unsigned char *) D_00AB9248,
    (unsigned char *) D_00AB9240, (unsigned char *) D_00AB9238,
    (unsigned char *) D_00AB9230, (unsigned char *) D_00AB9228,
    (unsigned char *) D_00AB9220, (unsigned char *) D_00AB9218,
    (unsigned char *) D_00AB9210, (unsigned char *) D_00AB9208,
    (unsigned char *) D_00AB9200, (unsigned char *) D_00AB91F8,
    (unsigned char *) D_00AB91F0, (unsigned char *) D_00AB91E8,
    (unsigned char *) D_00AB91E0, (unsigned char *) D_00AB91D8,
    (unsigned char *) D_00AB91D0, (unsigned char *) D_00AB91C8,
    (unsigned char *) D_00AB91C0, (unsigned char *) D_00AB91B8,
    (unsigned char *) D_00AB91B0, (unsigned char *) D_00AB91A8,
    (unsigned char *) D_00AB91A0, (unsigned char *) D_00AB9198,
    (unsigned char *) D_00AB9190, (unsigned char *) D_00AB9188,
    (unsigned char *) D_00AB9180, (unsigned char *) D_00AB9178,
    (unsigned char *) D_00AB9170, (unsigned char *) D_00AB9168,
    (unsigned char *) D_00AB9160, (unsigned char *) D_00AB9158,
    (unsigned char *) D_00AB9150, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    (unsigned char *) D_00AB9140, (unsigned char *) D_00AB9130,
    (unsigned char *) D_00AB9120, (unsigned char *) D_00AB9118,
    (unsigned char *) D_00AB9108, (unsigned char *) D_00AB90F8,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    (unsigned char *) D_00AB90E8, (unsigned char *) D_00AB90D8,
    (unsigned char *) D_00AB90C8, (unsigned char *) D_00AB90B8,
    0, 0,
    0, 0,
    0, 0,
    (unsigned char *) D_00AB90A8, (unsigned char *) D_00AB9098,
    (unsigned char *) D_00AB9088, (unsigned char *) D_00AB9078,
    (unsigned char *) D_00AB9068, (unsigned char *) D_00AB9058,
    (unsigned char *) D_00AB9048, (unsigned char *) D_00AB9038,
    (unsigned char *) D_00AB9028, (unsigned char *) D_00AB9018,
    (unsigned char *) D_00AB9008, (unsigned char *) D_00AB8FF8,
    (unsigned char *) D_00AB8FE8, (unsigned char *) D_00AB8FD8,
    (unsigned char *) D_00AB8FC8, (unsigned char *) D_00AB8FB8,
    (unsigned char *) D_00AB8FA8, (unsigned char *) D_00AB8F98,
    (unsigned char *) D_00AB8F88, (unsigned char *) D_00AB8F78,
    (unsigned char *) D_00AB8F68, (unsigned char *) D_00AB8F60,
    (unsigned char *) D_00AB8F58, (unsigned char *) D_00AB8F50,
    (unsigned char *) D_00AB8F48, (unsigned char *) D_00AB8F40,
    (unsigned char *) D_00AB8F38, (unsigned char *) D_00AB8F30,
    (unsigned char *) D_00AB8F28, (unsigned char *) D_00AB8F20,
    (unsigned char *) D_00AB8F18, (unsigned char *) D_00AB8F10,
    (unsigned char *) D_00AB8F08, (unsigned char *) D_00AB8F00,
    (unsigned char *) D_00AB8EF8, (unsigned char *) D_00AB8EF0,
    (unsigned char *) D_00AB8EE8, (unsigned char *) D_00AB8ED8,
    (unsigned char *) D_00AB8EC8, (unsigned char *) D_00AB8EB8,
};

unsigned char *dataMapNameTbl[68] = {
    0, (unsigned char *) D_00AB97F0,
    (unsigned char *) D_00AB97E8, (unsigned char *) D_00AB97E0,
    (unsigned char *) D_00AB97D8, (unsigned char *) D_00AB97D0,
    (unsigned char *) D_00AB97C8, (unsigned char *) D_00AB97C0,
    (unsigned char *) D_00AB97B0, (unsigned char *) D_00AB97A0,
    (unsigned char *) D_00AB9790, (unsigned char *) D_00AB9780,
    (unsigned char *) D_00AB9770, (unsigned char *) D_00AB9760,
    (unsigned char *) D_00AB9758, (unsigned char *) D_00AB9750,
    (unsigned char *) D_00AB9748, (unsigned char *) D_00AB9740,
    (unsigned char *) D_00AB9738, (unsigned char *) D_00AB9730,
    (unsigned char *) D_00AB9728, (unsigned char *) D_00AB9720,
    (unsigned char *) D_00AB9718, (unsigned char *) D_00AB9710,
    (unsigned char *) D_00AB9708, (unsigned char *) D_00AB9700,
    (unsigned char *) D_00AB96F8, (unsigned char *) D_00AB96F0,
    (unsigned char *) D_00AB96E8, (unsigned char *) D_00AB96E0,
    (unsigned char *) D_00AB96D8, (unsigned char *) D_00AB96D0,
    (unsigned char *) D_00AB96C8, (unsigned char *) D_00AB96C0,
    (unsigned char *) D_00AB96B8, (unsigned char *) D_00AB96B0,
    (unsigned char *) D_00AB96A8, (unsigned char *) D_00AB96A0,
    (unsigned char *) D_00AB9698, (unsigned char *) D_00AB9690,
    (unsigned char *) D_00AB9688, (unsigned char *) D_00AB9680,
    (unsigned char *) D_00AB9670, (unsigned char *) D_00AB9660,
    (unsigned char *) D_00AB9650, (unsigned char *) D_00AB9640,
    (unsigned char *) D_00AB9630, (unsigned char *) D_00AB9620,
    (unsigned char *) D_00AB9610, (unsigned char *) D_00AB9600,
    (unsigned char *) D_00AB95F0, (unsigned char *) D_00AB95E0,
    (unsigned char *) D_00AB95D0, (unsigned char *) D_00AB95C8,
    (unsigned char *) D_00AB95C0, (unsigned char *) D_00AB95B0,
    (unsigned char *) D_00AB95A8, (unsigned char *) D_00AB9598,
    (unsigned char *) D_00AB9590, (unsigned char *) D_00AB9588,
    (unsigned char *) D_00AB9580, (unsigned char *) D_00AB9578,
    (unsigned char *) D_00AB9570, (unsigned char *) D_00AB9568,
    (unsigned char *) D_00AB9558, (unsigned char *) D_00AB9550,
    (unsigned char *) D_00AB9540, (unsigned char *) D_00AB9538,
};

char *dataVoiceNameTbl[142] = {
    0, (char *) D_00AB9828,
    (char *) D_00AB9820, (char *) D_00AB9818,
    (char *) D_00AB9810, (char *) D_00AB9808,
    (char *) D_00AB9800, (char *) D_00AB97F8,
    0, 0,
    0, (char *) D_00AB94F8,
    (char *) D_00AB94F0, (char *) D_00AB94E8,
    0, 0,
    (char *) D_00AB94E0, (char *) D_00AB94D8,
    (char *) D_00AB94D0, (char *) D_00AB94C8,
    (char *) D_00AB94C0, (char *) D_00AB94B8,
    (char *) D_00AB94B0, (char *) D_00AB94A8,
    (char *) D_00AB94A0, (char *) D_00AB9498,
    (char *) D_00AB9490, (char *) D_00AB9488,
    (char *) D_00AB9480, (char *) D_00AB9478,
    (char *) D_00AB9470, (char *) D_00AB9468,
    (char *) D_00AB9460, (char *) D_00AB9458,
    (char *) D_00AB9450, (char *) D_00AB9448,
    (char *) D_00AB9440, (char *) D_00AB9438,
    (char *) D_00AB9430, (char *) D_00AB9428,
    (char *) D_00AB9420, (char *) D_00AB9418,
    (char *) D_00AB9410, (char *) D_00AB9408,
    (char *) D_00AB9400, (char *) D_00AB93F8,
    (char *) D_00AB93F0, (char *) D_00AB93E8,
    (char *) D_00AB93E0, (char *) D_00AB93D8,
    (char *) D_00AB93D0, (char *) D_00AB93C8,
    (char *) D_00AB93C0, (char *) D_00AB93B8,
    (char *) D_00AB93B0, (char *) D_00AB93A8,
    (char *) D_00AB93A0, (char *) D_00AB9398,
    (char *) D_00AB9390, (char *) D_00AB9388,
    (char *) D_00AB9380, (char *) D_00AB9378,
    (char *) D_00AB9370, (char *) D_00AB9368,
    (char *) D_00AB9360, (char *) D_00AB9358,
    (char *) D_00AB9350, (char *) D_00AB9348,
    (char *) D_00AB9340, (char *) D_00AB9338,
    (char *) D_00AB9330, (char *) D_00AB9328,
    (char *) D_00AB9320, (char *) D_00AB9318,
    (char *) D_00AB9310, (char *) D_00AB9308,
    (char *) D_00AB9300, (char *) D_00AB92F8,
    (char *) D_00AB92F0, (char *) D_00AB92E8,
    (char *) D_00AB92E0, (char *) D_00AB92D8,
    (char *) D_00AB92D0, (char *) D_00AB92C8,
    (char *) D_00AB92C0, (char *) D_00AB92B8,
    (char *) D_00AB92B0, (char *) D_00AB92A8,
    (char *) D_00AB92A0, (char *) D_00AB9298,
    (char *) D_00AB9290, (char *) D_00AB9288,
    (char *) D_00AB9280, (char *) D_00AB9278,
    (char *) D_00AB9270, (char *) D_00AB9268,
    (char *) D_00AB92D8, (char *) D_00AB92D0,
    (char *) D_00AB93B0, (char *) D_00AB9428,
    (char *) D_00AB9398, (char *) D_00AB92B8,
    (char *) D_00AB92B0, (char *) D_00AB92A8,
    (char *) D_00AB9390, (char *) D_00AB9360,
    (char *) D_00AB9260, (char *) D_00AB9258,
    (char *) D_00AB9250, (char *) D_00AB9248,
    (char *) D_00AB9240, (char *) D_00AB9238,
    (char *) D_00AB9230, (char *) D_00AB9228,
    (char *) D_00AB9220, (char *) D_00AB9218,
    (char *) D_00AB9210, (char *) D_00AB9208,
    (char *) D_00AB9200, (char *) D_00AB91F8,
    (char *) D_00AB91F0, (char *) D_00AB91E8,
    (char *) D_00AB91E0, (char *) D_00AB91D8,
    (char *) D_00AB91D0, (char *) D_00AB91C8,
    (char *) D_00AB91C0, (char *) D_00AB91B8,
    (char *) D_00AB91B0, (char *) D_00AB91A8,
    (char *) D_00AB91A0, (char *) D_00AB9198,
    (char *) D_00AB9190, (char *) D_00AB9188,
    (char *) D_00AB9180, (char *) D_00AB9178,
    (char *) D_00AB9170, (char *) D_00AB9168,
    (char *) D_00AB9160, (char *) D_00AB9158,
    (char *) D_00AB9150, 0,
};

char *D_00AB4C60[198] = {
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
    0, (char *) D_00AB9D10,
    (char *) D_00AB9D00, (char *) D_00AB9CF0,
    (char *) D_00AB9CE0, 0,
    0, 0,
    0, 0,
    0, (char *) D_00AB8F60,
    (char *) D_00AB8F58, (char *) D_00AB8F50,
    (char *) D_00AB8F48, (char *) D_00AB8F40,
    (char *) D_00AB8F38, (char *) D_00AB8F30,
    (char *) D_00AB8F28, (char *) D_00AB8F20,
    (char *) D_00AB8F18, (char *) D_00AB8F10,
    (char *) D_00AB8F08, (char *) D_00AB8F00,
    (char *) D_00AB8EF8, (char *) D_00AB8EF0,
    (char *) D_00AB8EE8, (char *) D_00AB9CD8,
    (char *) D_00AB9CD0, (char *) D_00AB9CC8,
    (char *) D_00AB9CC0, (char *) D_00AB9CB8,
    (char *) D_00AB9CB0, (char *) D_00AB9CA8,
    (char *) D_00AB9CA0, (char *) D_00AB9C98,
    (char *) D_00AB9C90, (char *) D_00AB9C88,
    (char *) D_00AB9C80, (char *) D_00AB9C78,
    (char *) D_00AB9C70, (char *) D_00AB9C68,
    (char *) D_00AB9C60, (char *) D_00AB9C58,
    (char *) D_00AB9C50, (char *) D_00AB9C48,
    (char *) D_00AB9C40, (char *) D_00AB9C38,
    (char *) D_00AB9C30, (char *) D_00AB9C28,
    (char *) D_00AB9C20, (char *) D_00AB9C18,
    (char *) D_00AB9C10, (char *) D_00AB9C08,
    (char *) D_00AB9C00, (char *) D_00AB9BF8,
    (char *) D_00AB9BF0, (char *) D_00AB9BE8,
    (char *) D_00AB9BE0, (char *) D_00AB9BD8,
    (char *) D_00AB9BD0, (char *) D_00AB9BC8,
    (char *) D_00AB9BC0, (char *) D_00AB9BB8,
    (char *) D_00AB9BB0, (char *) D_00AB9BA8,
    (char *) D_00AB9BA0, (char *) D_00AB9B98,
    (char *) D_00AB9B90, (char *) D_00AB9B88,
    (char *) D_00AB9B80, (char *) D_00AB9B78,
    (char *) D_00AB9B70, (char *) D_00AB9B68,
    (char *) D_00AB9B60, (char *) D_00AB9B58,
    (char *) D_00AB9B50, (char *) D_00AB9B48,
    (char *) D_00AB9B40, (char *) D_00AB9B38,
    (char *) D_00AB9B30, (char *) D_00AB9B28,
    (char *) D_00AB9B20, (char *) D_00AB9B18,
    (char *) D_00AB9B10, (char *) D_00AB9B08,
    (char *) D_00AB9B00, (char *) D_00AB9AF8,
    (char *) D_00AB9AF0, (char *) D_00AB9AE8,
    (char *) D_00AB9AE0, (char *) D_00AB9AD8,
    (char *) D_00AB9AD0, (char *) D_00AB9AC8,
    (char *) D_00AB9AC0, (char *) D_00AB9AB8,
    (char *) D_00AB9AB0, (char *) D_00AB9AA8,
    (char *) D_00AB9AA0, (char *) D_00AB9A98,
    (char *) D_00AB9A90, (char *) D_00AB9A88,
    (char *) D_00AB9A80, (char *) D_00AB9A78,
    (char *) D_00AB9A70, (char *) D_00AB9A68,
    (char *) D_00AB9A60, (char *) D_00AB9A58,
    (char *) D_00AB9A50, (char *) D_00AB9A48,
    (char *) D_00AB9A40, (char *) D_00AB9A38,
    (char *) D_00AB9A30, (char *) D_00AB9A28,
    (char *) D_00AB9A20, (char *) D_00AB9A18,
    (char *) D_00AB9A10, (char *) D_00AB9A08,
    (char *) D_00AB9A00, (char *) D_00AB99F8,
    (char *) D_00AB99F0, (char *) D_00AB99E8,
    (char *) D_00AB99E0, (char *) D_00AB99D8,
    (char *) D_00AB99D0, (char *) D_00AB99C8,
    (char *) D_00AB99C0, (char *) D_00AB99B8,
    (char *) D_00AB99B0, (char *) D_00AB99A8,
    (char *) D_00AB99A0, (char *) D_00AB9998,
    (char *) D_00AB9990, (char *) D_00AB9988,
    (char *) D_00AB9980, (char *) D_00AB9978,
    (char *) D_00AB9970, (char *) D_00AB9968,
    (char *) D_00AB9960, (char *) D_00AB9958,
    (char *) D_00AB9950, (char *) D_00AB9948,
    (char *) D_00AB9940, (char *) D_00AB9938,
    (char *) D_00AB9930, (char *) D_00AB9928,
    (char *) D_00AB9920, (char *) D_00AB9918,
    (char *) D_00AB9910, (char *) D_00AB9908,
    (char *) D_00AB9900, (char *) D_00AB98F8,
    (char *) D_00AB98F0, (char *) D_00AB98E8,
    (char *) D_00AB98E0, (char *) D_00AB98D8,
    (char *) D_00AB98D0, (char *) D_00AB98C8,
    (char *) D_00AB98C0, (char *) D_00AB98B8,
    (char *) D_00AB98B0, (char *) D_00AB98A8,
    (char *) D_00AB98A0, (char *) D_00AB9898,
    (char *) D_00AB9890, (char *) D_00AB9888,
    (char *) D_00AB9880, (char *) D_00AB9878,
    (char *) D_00AB9870, (char *) D_00AB9868,
    (char *) D_00AB9860, (char *) D_00AB9858,
    (char *) D_00AB9850, (char *) D_00AB9848,
    (char *) D_00AB9840, (char *) D_00AB9838,
    (char *) D_00AB9830, 0,
};

char *D_00AB4F78[140] = {
    (char *) D_00AB9F30, 0,
    (char *) D_00AB9F20, (char *) D_00AB9F10,
    (char *) D_00AB9F00, 0,
    0, 0,
    0, (char *) D_00AB9EF0,
    0, 0,
    0, 0,
    0, 0,
    0, (char *) D_00AB9EE0,
    0, 0,
    0, (char *) D_00AB9ED0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    (char *) D_00AB9EC0, 0,
    0, 0,
    0, (char *) D_00AB9EB0,
    0, 0,
    0, 0,
    0, 0,
    0, (char *) D_00AB9EA0,
    0, 0,
    0, (char *) D_00AB9E90,
    0, (char *) D_00AB9E80,
    (char *) D_00AB9E70, (char *) D_00AB9E60,
    0, 0,
    0, 0,
    (char *) D_00AB9E50, 0,
    0, 0,
    0, 0,
    0, 0,
    (char *) D_00AB9E40, 0,
    0, 0,
    (char *) D_00AB9E30, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, (char *) D_00AB9E20,
    0, 0,
    (char *) D_00AB9E10, 0,
    (char *) D_00AB9E00, (char *) D_00AB9DF0,
    (char *) D_00AB9DE0, 0,
    0, 0,
    0, 0,
    0, (char *) D_00AB9DD0,
    0, 0,
    (char *) D_00AB9DC0, 0,
    (char *) D_00AB9DB0, (char *) D_00AB9DA0,
    (char *) D_00AB9D90, 0,
    0, 0,
    0, (char *) D_00AB9D80,
    0, 0,
    0, 0,
    0, 0,
    0, (char *) D_00AB9D70,
    0, 0,
    0, (char *) D_00AB9D60,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    (char *) D_00AB9D50, 0,
    0, (char *) D_00AB9D40,
    0, (char *) D_00AB9D30,
    0, 0,
    0, 0,
};

char *dataBattleActionNameTbl[64] = {
    0, 0,
    0, (char *) D_00AB9D20,
    0, 0,
    0, 0,
    (char *) D_00ABA140, (char *) D_00ABA130,
    (char *) D_00ABA120, (char *) D_00ABA118,
    (char *) D_00ABA108, (char *) D_00ABA100,
    0, 0,
    (char *) D_00ABA0F0, (char *) D_00ABA0E0,
    (char *) D_00ABA0D0, (char *) D_00ABA0C8,
    (char *) D_00ABA0B8, (char *) D_00ABA0B0,
    0, 0,
    (char *) D_00ABA0A0, (char *) D_00ABA090,
    (char *) D_00ABA080, (char *) D_00ABA078,
    (char *) D_00ABA068, (char *) D_00ABA060,
    0, 0,
    (char *) D_00ABA050, (char *) D_00ABA040,
    (char *) D_00ABA030, (char *) D_00ABA028,
    (char *) D_00ABA018, (char *) D_00ABA010,
    0, 0,
    (char *) D_00ABA000, (char *) D_00AB9FF0,
    (char *) D_00AB9FE0, (char *) D_00AB9FD8,
    (char *) D_00AB9FC8, (char *) D_00AB9FC0,
    0, 0,
    (char *) D_00AB9FB0, (char *) D_00AB9FA0,
    (char *) D_00AB9F90, (char *) D_00AB9F88,
    (char *) D_00AB9F78, (char *) D_00AB9F70,
    0, 0,
    (char *) D_00AB9F68, (char *) D_00AB9F60,
    (char *) D_00AB9F58, (char *) D_00AB9F50,
    (char *) D_00AB9F48, (char *) D_00AB9F40,
    0, 0,
};

struct BattleNameSet dataBattleBstSets[7] = {
    { (char *) D_00ABA180, { 1, 1, 1, 1, 6, 0, 0, 0, 0, 0, 0 } },
    { (char *) D_00ABA178, { 1, 5, 16, 6, 0, 0, 0, 0, 0, 0, 0 } },
    { (char *) D_00ABA170, { 1, 1, 1, 1, 2, 3, 3, 3, 7, 8, 15 } },
    { (char *) D_00ABA168, { 1, 1, 1, 6, 7, 8, 9, 10, 0, 0, 0 } },
    { (char *) D_00ABA160, { 1, 1, 1, 7, 0, 0, 0, 0, 0, 0, 0 } },
    { (char *) D_00ABA158, { 1, 1, 1, 7, 0, 0, 0, 0, 0, 0, 0 } },
    { (char *) D_00ABA150, { 1, 1, 4, 7, 11, 10, 0, 0, 0, 0, 0 } }
};

struct BattleNameSet dataBattleBedSets[7] = {
    { (char *) D_00ABA1B8, { 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 } },
    { (char *) D_00ABA1B0, { 1, 17, 18, 19, 17, 17, 12, 0, 0, 0, 0 } },
    { (char *) D_00ABA1A8, { 1, 1, 1, 12, 13, 0, 0, 0, 0, 0, 0 } },
    { (char *) D_00ABA1A0, { 1, 1, 1, 1, 14, 0, 0, 0, 0, 0, 0 } },
    { (char *) D_00ABA198, { 1, 1, 12, 1, 0, 0, 0, 0, 0, 0, 0 } },
    { (char *) D_00ABA190, { 1, 1, 1, 12, 0, 0, 0, 0, 0, 0, 0 } },
    { (char *) D_00ABA188, { 1, 1, 1, 1, 12, 0, 0, 0, 0, 0, 0 } }
};
