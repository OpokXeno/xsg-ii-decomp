#include "common.h"
#include "main/xenoActor.h"
#include "main/xgl_timer0.h"
#include "main/xgl_cd.h"
#include "main/res_get_path.h"
#include "main/game_data.h"
#include "ov01/sb_vm.h"
#include "main/game_movie.h"
#include "main/xgl_font.h"
#include "main/game_disk_change.h"
#include "main/game_over.h"
#include "main/TiMRPackage.h"
#include "main/undulate.h"
#include "main/sef.h"
#include "main/xgl_2.h"

/* Returns 0. Same 8-byte body as uwamonoUnusedReturnZero in main/uwamono_light_blend.c.
 * No caller found in the binary. */
int resUnusedReturnZero(void) {
    return 0;
}

/*
 * Footstep material keys (proposed footMaterialKeys): 4-byte entries holding three letters, the fourth byte zero.
 * The list ends at an entry whose first byte is zero. Read by footMaterialFind and footMaterialKey.
 */
extern int footMaterialKeys[16];

/*
 * Returns the key entry at `index` (proposed footMaterialKey). enefoot and seFootBanksLoad copy its three bytes into
 * their sound command.
 */
void *footMaterialKey(int index) {
    return &footMaterialKeys[index];
}

/*
 * Searches the key list for the three bytes of `name` (proposed footMaterialFind) and returns the matching index.
 * It returns 0 when the list ends without a match, which is also the index of "kon", the default material.
 * foot calls it once per token.
 */
int footMaterialFind(const char *name) {
    const char *entry = (const char *)footMaterialKeys;
    int index = 0;

    while (entry[0] != 0) {
        if (entry[0] == name[0] && entry[1] == name[1] && entry[2] == name[2]) {
            return index;
        }
        entry += 4;
        index++;
    }
    return 0;
}

/*
 * The CD file name tables: per group of file ids (xglCdFileGroups) the names of its files, then the
 * XAP groups. The names longer than 8 bytes are .rodata, the others .sdata (a -G8 file); each table
 * holds them in reverse address order.
 */


char *xglCdPcModelNames[58] = {
    "model\\char\\pc\\", "shionH00", "shion00", "_",
    "shion05", "shion03", "shion04", "shionH03",
    "sh_box01", "kosmosH00", "kosmos00", "kosmos01",
    "kosmos02", "kosmos03", "kos_sw03", "chaosH00",
    "chaos00", "chaos01", "chaos02", "chaos03",
    "chaosH01", "jrH00", "jr00", "jr01",
    "jr02", "jr03", "jrH01", "momoH00",
    "momo00", "momo01", "momo02", "momoH03",
    "mm_ym01", "_", "ziggy02", "ziggy00",
    "ziggy01", "jinH00", "jin00", "jin01",
    "jin02", "jinH01", "jin_saya", "jin_sw01",
    "jrH02", "jrH04", "jinH03", "jinH04",
    "_", "jr_gun01", "jr_gun01b", "jr_gun01b_2",
    "zig_sw01", "jin_sw02", "jin_sw03", "jin_sw04",
    "chaosH02", "ziggyH01"
};

char *xglCdNpcModelNames[90] = {
    "model\\char\\npc\\", "allenH01", "allen01", "albelt00",
    "albelt01", "albelt02", "albelt03", "helmer00",
    "helmer01", "marglis00", "marglis01", "pope00",
    "pelegri00", "kebin00", "kebin01", "gaignun00",
    "gaignun01", "gaignun02", "shelley00", "mary00",
    "yuri00", "yuri01", "sakura00", "_",
    "sellers00", "yuriev00", "_", "voyager00",
    "voyager01", "citren00", "febronia00", "_",
    "_", "_", "_", "virgil00",
    "_", "_", "_", "_",
    "_", "_", "_", "_",
    "_", "kirsh00", "kirsh01", "_",
    "nephilim00", "miyuki00", "_", "tonny00",
    "hammer00", "mathews00", "_", "_",
    "cananH00", "canan01", "_", "_",
    "virhelm00", "albeltH00", "albeltH01", "albeltH03",
    "albeltH04", "albeltH05", "marglisH00", "marglisH01",
    "marglisH02", "bos013", "bos001", "popeH00",
    "gaignunH00", "gaignunH02", "gaignunH04", "gaignun03",
    "yuriH00", "yuriH01", "sakuraH00", "nephilim01",
    "tonnyH00", "hammerH00", "mathewsH00", "albeltH06",
    "albeltH07", "marglisH03", "gaignunH01", "gaignunH03",
    "gaignunH05", "gaignunH06"
};

char *xglCdCfNpcModelNames[88] = {
    "model\\char\\cf_npc\\", "vecm100", "vecm200", "vecf100",
    "vecf200", "vecm300", "vecf300", "_",
    "gfgm00", "gfgf00", "urtvmH00", "urtvm00",
    "urtvf00", "uticm00", "cf_uso001", "cf_uso002",
    "gfgass100", "gfgass200", "gfgass300", "engm01",
    "engm02", "denki00", "water00", "gomi00",
    "du100", "du200", "hyaku00", "gfgm01",
    "gfgm02", "gfgf01", "gfgf02", "du101",
    "du102", "du201", "du202", "realm00",
    "realf00", "brealm00", "brealm01", "engm00",
    "hakase01", "hakase00", "scott00", "inngirl00",
    "mouth00", "king00", "bread00", "breadch00",
    "clean00", "cleanch00", "pelesol00", "master00",
    "hakase02", "vectech00", "umnm00", "umnf00",
    "yurim00", "cf_uso003", "cf_uso004", "cf_uso007",
    "cf_uso008", "cf_uso009", "cf_uso010", "bar00",
    "thief00", "thief01", "thief02", "thief03",
    "thief04", "org00", "cf_bos010", "uso011",
    "uso012", "bos030", "uso005", "uso006",
    "umnm01", "umnf01", "umnmh00", "umnfh00",
    "umnm02", "umnf02", "vecm400", "vecf400",
    "gfgm03", "gfgf03", "du103", "umnm03"
};

char *xglCdCitizenModelNames[72] = {
    "model\\char\\citizen\\", "kidm100", "kidm101", "kidm200",
    "kidm201", "kidf100", "kidf101", "kidf200",
    "kidf201", "boy100", "boy101", "boy200",
    "boy201", "boy300", "boy301", "boy400",
    "boy401", "boy500", "boy501", "boy600",
    "boy601", "girl100", "girl101", "girl200",
    "girl201", "girl300", "girl301", "girl400",
    "girl401", "girl500", "girl501", "kidm300",
    "kidf300", "man100", "man101", "man200",
    "man201", "man300", "man301", "man400",
    "man401", "woman100", "woman101", "woman200",
    "woman201", "woman300", "woman301", "woman400",
    "woman401", "oldm100", "oldm101", "oldm200",
    "oldm201", "oldf100", "oldf101", "oldf200",
    "oldf201", "ukun00", "robo00", "doctor00",
    "spy00", "singer00", "cook100", "cook200",
    "kingcat00", "kitten00", "cat00", "dog00",
    "rat00", "ballet00", "robo01", "robo02"
};

char *xglCdPcRoboModelNames[20] = {
    "model\\robo\\pc_robo\\", "pcr01a", "pcr02a", "pcr03",
    "_", "pcr05", "pcr05_sh01", "pcr05_mg01",
    "pcr02b", "pcr02c", "pcr02d", "pcr05b",
    "pcr02_rp01", "pcr02_rp02", "pcr05_rf01", "pcr05_sh01",
    "pcr05_br01", "pcr05_sr01", "pcr01b", 0
};

char *xglCdNpcRoboModelNames[34] = {
    "model\\robo\\npc_robo\\", "npcr01", "npcr02a", "npcr02b",
    "npcr03", "npcr04", "npcr05", "npcr06",
    "npcr07", "npcr08", "npcr09a", "npcr10",
    "npcr11", "npcr12", "npcr13", "npcr14",
    "npcr15", "npcr16", "npcr17", "npcr18",
    "npcr19", "npcr09b", "npcr12a", "npcr12b",
    "npcr12_rf01", "npcr12_mg01", "npcr13_sw01", "npcr06_rf01",
    "npcr06_mg01", "npcr06_sw01", "npcr17_sw01", "npcr10_rf01",
    "npcr10_sh01", 0
};

char *xglCdGattaiRoboModelNames[82] = {
    "model\\robo\\gattai_robo\\", "gattai01", "gattai02", "gattai03",
    "gattai04", "gattai05", "gattai06", "gattai07",
    "gattai08", "gattai09", "gattai10", "gattai11",
    "gattai12", "gattai13", "gattai14", "gattai15",
    "gattai16", "gattai17", "gattai18", "gattai19",
    "gattai20", "gattai21", "gattai22", "gattai23",
    "gattai24", "gattai25", "gattai26", "gattai27",
    "gattai28", "gattai29", "gattai30", "gattai31",
    "gattai32", "gattai33", "gattai34", "gattai35",
    "gattai36", "gattai37", "gattai38", "gattai39",
    "gattai40", "gattai41", "gattai42", "gattai43",
    "gattai44", "gattai45", "gattai46", "gattai47",
    "gattai48", "gattai49", "gattai50", "gattai51",
    "gattai52", "gattai53", "gattai54", "gattai55",
    "gattai56", "gattai57", "gattai58", "gattai59",
    "gattai60", "gattai61", "gattai62", "gattai63",
    "gattai64", "gattai65", "gattai66", "gattai67",
    "gattai68", "gattai69", "gattai70", "gattai71",
    "gattai72", "gattai73", "gattai74", "gattai75",
    "gattai76", "gattai77", "gattai78", "gattai79",
    "gattai80", "gattai81"
};

char *xglCdEnemyModelNames[84] = {
    "model\\enemy\\", "uso001", "uso002", "uro001",
    "uma001", "uma002", "uso003", "uso004",
    "uma003", "uro002", "uma004", "uma005",
    "uma006", "cre001", "cre002", "cre003",
    "cre004", "cre005", "cre006", "cre007",
    "cre008", "cre009", "cre010", "vma001",
    "vma002", "vma003", "uro003", "uro004",
    "uma007", "uro005", "uro006", "uro007",
    "uso005", "uso006", "uma008", "uso007",
    "uso008", "cre011", "cre012", "cre018",
    "uro008", "uro009", "uma009", "uma010",
    "uro010", "uso009", "uso010", "cre013",
    "cre014", "uma012", "uro011", "uro012",
    "uro013", "uro014", "uro015", "uso011",
    "uso012", "cre015", "cre016", "cre017",
    "uma011", "gno001", "gno002", "gno003",
    "gno004", "gno005", "gno006", "gno007",
    "gno008", "gno009", "gno010", "gno011",
    "gno012", "gno013", "gno014", "gno015",
    "gno016", "gno017", "gno018", "gno019",
    "uso001_rf01", "uso001_sg01", "uma013", 0
};

char *xglCdBossModelNames[32] = {
    "model\\enemy\\", "bos001", "bos002", "bos003",
    "bos004", "bos005", "bos006", "bos007",
    "bos008", "bos009", "bos010", "bos011",
    "bos012", "bos013", "bos014", "bos015",
    "bos016", "bos017", "bos018", "bos019",
    "bos020", "bos021", "bos022", "bos023",
    "bos024", "bos025", "bos026", "bos027",
    "bos028", "bos029", "bos030", "bos031"
};

char *xglCdMapModelNames[362] = {
    "model\\map\\", "kowashi_chara", "kowashi_robo", "idtest",
    "camera_test", "E2_TST01", "E2_TST02", "E2_TST03",
    "E2_TST04", "E2_TST05", "E2_TST06", "dummy_0b",
    "dummy_0c", "dummy_0d", "dummy_0e", "dummy_0f",
    "E2_KML01", "E2_KML02", "E2_KML03", "E2_KML04",
    "E2_KML05", "E2_KML06", "E2_KML07", "E2_KML08",
    "E2_KML09", "E2_KML10", "E2_MID01", "E2_MID02",
    "E2_MID03", "E2_MID04", "E2_MID05", "E2_MID06",
    "E2_MID07", "E2_MID08", "E2_MID09", "E2_VEC01",
    "E2_VEC02", "E2_VEC03", "E2_MIL01", "E2_MIL02",
    "E2_MIL03", "E2_MIL04", "E2_MIL05", "E2_MIL06",
    "E2_MIL07", "E2_MIL08", "E2_MIL09", "E2_MIL10",
    "E2_MIL11", "E2_MIL12", "E2_MIL13", "E2_MIL14",
    "E2_MIL15", "E2_MIL16", "E2_MIL17", "E2_UZU01",
    "E2_UZU06", "E2_UZU08", "E2_UMN01", "E2_UMN02",
    "E2_UMN03", "E2_UMN04", "E2_MOM01", "E2_MOM02",
    "E2_MOM03", "E2_MOM05", "E2_MOM06", "E2_MOM07",
    "E2_MOM08", "E2_MOM09", "E2_MOM10", "E2_MOM11",
    "E2_MOM12", "E2_MOM13", "E2_MOM14", "E2_MOM15",
    "E2_MOM16", "E2_MOM17", "E2_MOM18", "E2_MOM19",
    "E2_MOM21", "E2_MOM22", "E2_MOM23", "E2_MOM24",
    "E2_MOM25", "E2_MOM26", "E2_MOM27", "E2_MOM28",
    "E2_ELS01", "E2_ELS02", "E2_ELS03", "E2_ELS04",
    "E2_ELS05", "E2_ELS06", "E2_ELS07", "E2_ELS08",
    "E2_SYD01", "E2_SYD02", "E2_SYD03", "E2_SYD04",
    "E2_SYD05", "E2_SYD06", "E2_SYD07", "E2_SYD08",
    "E2_SYD09", "E2_SYD10", "E2_SYD11", "E2_SYD12",
    "E2_SYD13", "E2_SYD14", "E2_SYD15", "E2_SYD16",
    "E2_SYD17", "E2_SYD18", "E2_SYD19", "E2_SYD20",
    "E2_SPA01", "E2_OLM01", "E2_OLM02", "E2_OLM03",
    "E2_OLM04", "E2_OLM05", "E2_OLM06", "E2_OLM07",
    "E2_OLM08", "E2_OLM09", "E2_OLM10", "E2_OLM11",
    "E2_OLM12", "E2_OLM13", "E2_OLM14", "E2_OLM15",
    "E2_OLM16", "E2_OLM17", "E2_OLM18", "E2_DYU01",
    "E2_DYU02", "E2_DYU03", "E2_DYU04", "E2_DYU05",
    "E2_KUK01", "E2_KUK02", "E2_KUK03", "E2_KUK04",
    "E2_KUK05", "E2_KUK06", "E2_KUK07", "E2_KUK08",
    "E2_KUK09", "E2_KUK10", "E2_KUK11", "E2_KUK17",
    "E2_KUK18", "E2_KUK19", "E2_SUI01", "E2_SUI02",
    "E2_SUI03", "E2_SUI04", "E2_SUI05", "E2_SUI06",
    "E2_SUI07", "E2_SUI08", "E2_SUI09", "E2_SUI10",
    "E2_SUI11", "E2_SUI12", "E2_RAV01", "E2_RAV02",
    "E2_RAV03", "E2_RAV04", "E2_RAV05", "E2_RAV06",
    "E2_RAV07", "E2_RAV08", "E2_RAV09", "E2_RAV10",
    "E2_RAV11", "E2_RAV12", "E2_RAV13", "E2_RAV14",
    "E2_RAV15", "E2_OMG01", "E2_OMG02", "E2_OMG03",
    "E2_OMG04", "E2_OMG05", "E2_OMG06", "E2_OMG07",
    "E2_OMG08", "E2_OMG09", "E2_OMG10", "E2_OMG11",
    "E2_OMG12", "E2_OMG13", "E2_OMG14", "E2_OMG15",
    "E2_OMG16", "E2_OMG17", "E2_OMG18", "E2_OMG19",
    "E2_OMG20", "E2_JSY01", "E2_JSY02", "E2_JSY03",
    "E2_JSY04", "E2_JSY05", "E2_JSY06", "E2_JSY07",
    "E2_JSY08", "E2_EXA01", "E2_EXA02", "E2_EXA03",
    "E2_EXA04", "E2_EXA05", "E2_EXA06", "E2_EXA07",
    "E2_EXB01", "E2_EXB02", "E2_EXB03", "E2_EXB04",
    "E2_EXB05", "E2_EXB06", "E2_EXB07", "E2_EXC01",
    "E2_EXC02", "E2_EXC03", "E2_EXC04", "E2_EXC05",
    "E2_EXC06", "E2_EXC07", "E2_EXC08", "E2_EXC09",
    "E2_EXC10", "E2_EXC11", "E2_EXD01", "CFEVE01",
    "CFEVE02", "CFEVE03", "CFEVE04", "CFEVE05",
    "CFEVE06", "CFEVE07", "CFEVE08", "CFEVE09",
    "CFEVE10", "CFEVE11", "CFEVE12", "CFEVE13",
    "CFEVE14", "CFEVE15", "E2_EVE01", "E2_EVE02",
    "E2_EVE03", "E2_EVE04", "E2_EVE05", "E2_EVE06",
    "E2_EVE07", "E2_EVE08", "E2_EVE09", "E2_EVE10",
    "E2_EVE11", "E2_EVE12", "E2_EVE13", "E2_EVE14",
    "E2_EVE15", "E2_EVE16", "E2_EVE17", "E2_EVE18",
    "E2_EVE19", "E2_EVE20", "E2_EVE21", "E2_EVE22",
    "E2_EVE23", "E2_EVE24", "E2_EVE25", "E2_EVE26",
    "E2_EVE27", "E2_EVE28", "E2_EVE29", "E2_EVE30",
    "E2_EVE31", "E2_EVE32", "E2_EVE33", "E2_EVE34",
    "E2_EVE35", "E2_EVE36", "E2_EVE37", "E2_EVE38",
    "E2_EVE39", "E2_EVE40", "E2_EVE41", "E2_EVE42",
    "E2_EVE43", "E2_EVE44", "E2_EVE45", "E2_EVE46",
    "E2_EVE47", "E2_EVE48", "E2_EVE49", "E2_EVE50",
    "E2_EVE51", "E2_EVE52", "E2_EVE53", "E2_EVE54",
    "E2_EVE55", "E2_EVE56", "E2_EVE57", "E2_EVE58",
    "E2_EVE59", "E2_EVE60", "E2_EVE61", "E2_EVE62",
    "E2_EVE63", "E2_EVE64", "E2_EVE65", "E2_EVE66",
    "E2_EVE67", "E2_EVE68", "E2_EVE69", "E2_EVE70",
    "E2_EVE71", "E2_EVE72", "E2_EVE73", "E2_EVE74",
    "E2_EVE75", "E2_EVE76", "E2_EVE77", "E2_EVE78",
    "E2_EVE79", "E2_EVE80", "E2_EVE81", "E2_EVE82",
    "E2_EVE83", "E2_EVE84", "E2_EVE85", "E2_EVE86",
    "E2_EVE87", "E2_EVE88", "E2_EVE89", "E2_EVE90",
    "E2_EVE91", "E2_EVS01", "E2_EVS02", "E2_EVS02b",
    "E2_EVS03", "E2_EVS03b", "E2_EVS04", "E2_EVS05",
    "E2_EVS06", "E2_EVS07", "E2_EVS08", "E2_EVS09",
    "E2_MOM04", "E2_KML11", "CFEVE16", "CFEVE17",
    "CFEVE18", "CFEVE19", "E2_OMG21", "CFEVE20",
    "CFEVE21", "CFEVE22"
};

char *xglCdMotionNames[66] = {
    "motion\\", "chaos00", "jin00", "shion00",
    "momo00", "ziggy00", "jr00", "kosmos00",
    "kanan00", "pcr00", "pcr01", "pcr02",
    "pcr03", "pcr04", "pcr05", "motion_ext01",
    "motion_ext02", "motion_ext03", "motion_ext04", "motion_ext05",
    "motion_ext06", "motion_ext07", "motion_ext08", "motion_ext09",
    "motion_ext10", "motion_ext11", "motion_ext12", "motion_ext13",
    "motion_ext14", "motion_ext15", "motion_ext16", "motion_ext17",
    "motion_ext18", "motion_ext19", "motion_ext20", "motion_ext21",
    "motion_ext22", "motion_ext23", "motion_ext24", "motion_ext25",
    "motion_ext26", "motion_ext27", "motion_ext28", "motion_ext29",
    "motion_ext30", "motion_ext31", "motion_ext32", "motion_ext33",
    "motion_ext34", "motion_ext35", "motion_ext36", "motion_ext37",
    "motion_ext38", "motion_ext39", "motion_ext40", "motion_ext41",
    "motion_ext42", "motion_ext43", "motion_ext44", "motion_ext45",
    "motion_ext46", "motion_ext47", "motion_ext48", "motion_ext49",
    "motion_ext50", "face_all"
};

char *xglCdEffectNames[10] = {
    "effect\\", "00", "01", "02",
    "03", "04", "05", "06",
    "07", 0
};

char *xglCdMobjModelNames[326] = {
    "model\\mobj\\", "mobj000", "mobj001", "mobj002",
    "mobj003", "mobj004", "mobj005", "mobj006",
    "mobj007", "mobj008", "mobj009", "mobj010",
    "mobj011", "mobj012", "mobj013", "mobj014",
    "mobj015a", "mobj015b", "mobj016", "mobj017",
    "mobj018", "mobj019", "mobj020", "mobj021",
    "mobj022", "mobj023", "mobj024", "mobj025",
    "mobj026", "mobj027", "mobj028a", "mobj028b",
    "mobj028c", "mobj029a", "mobj029b", "mobj029c",
    "mobj029d", "mobj029e", "mobj029f", "mobj030a",
    "mobj030b", "mobj030c", "mobj030d", "mobj030e",
    "mobj030f", "mobj031a", "mobj031b", "mobj031c",
    "mobj031d", "mobj031e", "mobj031f", "mobj032a",
    "mobj032b", "mobj033", "mobj034", "mobj035",
    "mobj036", "mobj037", "mobj038", "mobj039a",
    "mobj039b", "mobj040", "mobj041", "mobj042",
    "mobj043", "mobj044", "mobj045", "mobj046",
    "mobj047", "mobj048", "mobj049", "mobj050",
    "mobj051", "mobj052", "mobj053", "mobj054",
    "mobj055", "mobj056", "mobj057", "mobj058",
    "mobj059", "mobj060", "mobj061", "mobj062",
    "mobj063", "mobj064", "mobj065", "mobj066",
    "mobj067", "mobj068", "mobj069", "mobj070",
    "mobj071", "mobj072", "mobj072b", "mobj073",
    "mobj074", "mobj075", "mobj076", "mobj077",
    "mobj078", "mobj079", "mobj080", "mobj081",
    "mobj082", "mobj083", "mobj084", "mobj085",
    "mobj086", "mobj087", "mobj088", "mobj089",
    "mobj090", "mobj091", "mobj092", "mobj093",
    "mobj094", "mobj095", "mobj096", "mobj097",
    "mobj098", "mobj099", "mobj100", "mobj101",
    "mobj102", "mobj103", "mobj104", "mobj105",
    "mobj106", "mobj107", "mobj108", "mobj109",
    "mobj110", "mobj111", "mobj112", "mobj113",
    "mobj114", "mobj115", "mobj116", "mobj117",
    "mobj118", "mobj119", "mobj120", "mobj121",
    "mobj122", "mobj123", "mobj124", "mobj125",
    "mobj126", "mobj127", "mobj128", "mobj129",
    "mobj130", "mobj131", "mobj132", "mobj133",
    "mobj134", "mobj135", "mobj136", "mobj137",
    "mobj138", "mobj139", "mobj140", "mobj141",
    "mobj142", "mobj143", "mobj144", "mobj145",
    "mobj146", "mobj147", "mobj148", "mobj149",
    "mobj150", "mobj151", "mobj152", "_",
    "mobj154", "mobj155", "mobj156", "mobj157",
    "mobj158", "mobj159", "mobj160", "mobj161",
    "mobj162", "mobj163", "mobj164", "mobj165",
    "mobj166", "mobj167", "mobj168", "mobj169",
    "mobj170", "mobj171", "mobj172", "mobj173",
    "mobj174", "mobj175", "mobj176", "mobj177",
    "mobj178", "mobj179", "mobj180", "mobj181",
    "mobj182", "mobj183", "mobj184", "mobj185",
    "mobj186", "mobj187", "mobj188", "mobj189",
    "mobj190", "mobj191", "mobj192", "mobj193",
    "mobj194", "mobj195", "mobj196", "mobj197",
    "mobj198", "mobj199", "mobj200", "mobj201",
    "mobj202", "mobj203", "mobj204", "mobj205",
    "mobj206", "mobj207", "mobj208", "mobj209",
    "mobj210", "mobj211", "mobj212", "mobj213",
    "mobj214", "mobj215", "mobj216", "mobj217",
    "mobj218", "mobj219", "mobj220", "mobj221",
    "mobj222", "mobj223", "mobj224", "mobj225",
    "mobj226", "mobj227", "mobj228", "mobj229",
    "mobj230", "mobj231", "mobj232", "mobj233",
    "mobj234", "mobj235", "mobj236", "mobj237",
    "mobj238", "mobj239", "mobj240", "mobj241",
    "mobj242", "mobj243", "mobj244", "mobj245",
    "mobj246", "mobj247", "mobj248", "mobj249",
    "mobj250", "mobj251", "mobj252", "mobj253",
    "mobj254", "mobj255", "mobj256", "mobj257",
    "mobj258", "mobj259", "mobj260", "mobj261",
    "mobj262", "mobj263", "mobj264", "mobj265",
    "mobj266", "mobj267", "mobj268", "mobj269",
    "mobj270", "mobj271", "mobj272", "mobj273",
    "mobj274", "mobj275", "mobj276", "mobj277",
    "mobj278", "mobj279", "mobj280", "mobj281",
    "mobj282", "mobj283", "mobj284", "mobj285",
    "mobj286", "mobj287", "mobj288", "mobj289",
    "mobj290", "mobj291", "mobj292", "mobj293",
    "mobj294", "mobj295", "mobj296", "mobj297",
    "mobj298", "mobj299", "mobj153a", "mobj153b",
    "mobj153c", 0
};

char *xglCdCfEnemyNames[60] = {
    "cf\\", "enemy001", "enemy002", "enemy003",
    "enemy004", "enemy005", "enemy006", "enemy007",
    "enemy008", "enemy009", "enemy010", "enemy011",
    "enemy012", "enemy013", "enemy014", "enemy015",
    "enemy016", "enemy017", "enemy018", "enemy019",
    "enemy_001", "enemy_002", "enemy_003", "enemy_004",
    "enemy_005", "enemy_006", "enemy_007", "enemy_008",
    "enemy_009", "enemy_010", "enemy_011", "enemy_012",
    "enemy_013", "enemy_014", "enemy_015", "enemy_016",
    "enemy_017", "enemy_018", "enemy_019", "enemy_020",
    "npc_001", "npc_002", "npc_003", "npc_004",
    "npc_005", "npc_006", "npc_007", "npc_008",
    "npc_009", "npc_010", "npc_011", "npc_012",
    "npc_013", "npc_014", "npc_015", "npc_016",
    "npc_017", "npc_018", "npc_019", "npc_020"
};

/* One 0xC-byte entry of xglCdFileGroups: file ids base .. base + count - 1 map to
 * names[0 .. count - 1] (xglCdFindFileId, xglCdFindFileName); xglCdBuildFileName also writes
 * names[0] in front of the file's own name. An entry with no names ends the table. */
typedef struct CdFileGroup {
    unsigned int base;
    unsigned int count;
    char **names;
} CdFileGroup;

CdFileGroup xglCdFileGroups[15] = {
    { 0x10000, 58, xglCdPcModelNames },
    { 0x11000, 90, xglCdNpcModelNames },
    { 0x12000, 88, xglCdCfNpcModelNames },
    { 0x13000, 72, xglCdCitizenModelNames },
    { 0x14000, 19, xglCdPcRoboModelNames },
    { 0x15000, 33, xglCdNpcRoboModelNames },
    { 0x16000, 82, xglCdGattaiRoboModelNames },
    { 0x18000, 83, xglCdEnemyModelNames },
    { 0x19000, 32, xglCdBossModelNames },
    { 0x20000, 362, xglCdMapModelNames },
    { 0x30000, 66, xglCdMotionNames },
    { 0x40000, 9, xglCdEffectNames },
    { 0x70000, 325, xglCdMobjModelNames },
    { 0x80000, 60, xglCdCfEnemyNames },
    { 0xFFFFFFFF, 0, 0 }
};

/* The words of xglCdXapGroups are groups: a name, its file ids, then -1; a lone -1 closes the table. */
int xglCdXapGroups[138] = {
    (int) "man", 69656, 73774, 73779, -1, (int) "woman", 73776, -1,
    (int) "boy", 69692, 73767, 73770, 73749, 73750, -1, (int) "girl",
    69662, 73775, -1, (int) "oldm", 73751, -1, (int) "soldm", 69641,
    69642, 69643, 69667, 69668, 69669, 73736, 73755, 73756,
    73812, 73741, 73742, 73743, 73765, 73766, 73778, 73785,
    73786, 73787, 73788, 73789, 73790, 73784, 73799, 73800,
    -1, (int) "soldf", 69644, 73737, 73757, 73758, 73813, -1,
    (int) "kidm", 73777, 77855, -1, (int) "kidf", 73771, 77856, -1,
    (int) "albelt", 69657, -1, (int) "momo00", 69654, 69677, 69680, 73754,
    -1, (int) "kebin", 69659, 69660, 73797, 73798, -1, (int) "urtvf",
    69661, -1, (int) "proudm", 73744, 73745, 73746, -1, (int) "faithm",
    73781, -1, (int) "kenkyum", 73729, 73730, 73733, 73810, 73752,
    73759, 73760, 73814, 73763, 73782, 73804, 73808, 73815,
    -1, (int) "kenkyuf", 73731, 73732, 73734, 73811, 73753, 73761,
    73762, 73764, 73783, 73805, 73809, 77885, -1, (int) "erobo",
    77882, 77894, 77895, -1, (int) "cook1", 77886, -1, (int) "cook2",
    77887, -1, (int) "cat", 77888, 77889, -1, (int) "oldf", 77893,
    -1, -1
};

/* Footstep material keys: 16 entries of 4 bytes (three letters and a zero), terminated by an empty entry. */
int footMaterialKeys[16] = {
    0x006E6F6B, 0x00696172, 0x00616C67, 0x006F6C66,
    0x006F7269, 0x006F7473, 0x00726163, 0x006F6F77,
    0x006F6C63, 0x00746177, 0x00727574, 0x00726165,
    0x006F6E73, 0x00616E73, 0x00000000, 0x00000000,
};

/* Start addresses in sound memory of the banks, one per slot (passed to seBankLoad as its bank address; the
 * last one is zero). Guessed type. */
int seBankAddressTable[22] = {
    0x00019000, 0x00005000, 0x0007D000, 0x000BE000, 0x000BE000, 0x00000000,
    0x00000000, 0x00154000, 0x00176000, 0x0017A400, 0x0017E800, 0x00182C00,
    0x00187000, 0x0018D000, 0x00193000, 0x00199000, 0x0019F000, 0x001A5000,
    0x001AB000, 0x001B1000, 0x001B7000, 0x00000000,
};


/* Command table of resScriptRun: the name stored in the scaffold strings of res_get_path.c (its ';' prefix is not
 * part of the name) and the handler. */
typedef struct {
    signed char *name;
    unsigned char *(*handler)(unsigned char *cursor);
} ResCommand;

u8 *resCmdMap(u8 *cursor);
u8 *resCmdAct(u8 *cursor);
u8 *resCmdMot(u8 *cursor);
u8 *resCmdMobj(u8 *cursor);
u8 *resCmdEffect(u8 *cursor);
u8 *resCmdMapse(u8 *script);
u8 *resCmdRuntimeBgm(u8 *script);
u8 *resCmdBgmVol(u8 *cursor);
u8 *resCmdFoot(u8 *script);
u8 *resCmdEnefoot(u8 *script);
u8 *command_disk(u8 *entry);
u8 *command_save(u8 *cursor);
u8 *resCmdMpeg2(u8 *cursor);

char defaultXapName[8] = "man.xap";
char cfsSeBankName[8] = "CFS_SE";
char *cfsSeBankNamePointer = cfsSeBankName;

ResCommand resCommandTable[14] = {
    { "map", resCmdMap },
    { "act", resCmdAct },
    { "mot", resCmdMot },
    { "mobj", resCmdMobj },
    { "effect", resCmdEffect },
    { "mapse", resCmdMapse },
    { "bgm", resCmdRuntimeBgm },
    { "bgmvol", resCmdBgmVol },
    { "foot", resCmdFoot },
    { "enefoot", resCmdEnefoot },
    { "disk", command_disk },
    { "save", command_save },
    { ";mpeg2", resCmdMpeg2 },
    { 0, 0 }
};

char *cfDirectoryPath = "cf\\";


/* ---- res_get_path ---- */

/* Writes the low 16 bits of number as a 6-byte name: "cf" then four decimal digits, no NUL. */
void xglCdFormatFileName(char *buffer, int number)
{
    unsigned int remaining;
    int digit;

    remaining = number & 0xFFFF;
    buffer[0] = 'c';
    buffer[1] = 'f';
    for (digit = 0; digit < 4; digit++) {
        buffer[5 - digit] = remaining % 10U + '0';
        remaining = remaining / 10U;
    }
}

/* Block sizes are rounded up to this (GameResBlockAlloc). */
#define GAME_RES_BLOCK_ALIGN 0x40
/* A block loaded by name starts with the name in a header of this size (GameResLoadName). */
#define GAME_RES_NAME_HEADER_SIZE 0x40
/* File sizes are rounded up to whole CD sectors. */
#define CD_SECTOR_SIZE 0x800
/* Owner tag of a block allocated without an owner, and the mask selecting that tag. */
#define GAME_RES_OWNER 0xF00000
#define GAME_RES_OWNER_MASK 0xFF0000
/* Resource ids carry the file id in their low 24 bits (GameResLoadID). */
#define GAME_RES_ID_MASK 0xFFFFFF

/* Address range gameResArenaStart .. gameResArenaEnd whose blocks GameResBlockFindByOwner and GameResBlockFindByName skip. */
static void *gameResArenaStart;
static void *gameResArenaEnd;

/* The name key stored in a block header: name itself when it fits the header; for longer
 * names the original returns name - (GAME_RES_NAME_HEADER_SIZE - 1), not an offset into name. */
const char *gameResNameKey(const char *name)
{
    int length;

    for (length = 0; name[length] != 0; length++) {
    }
    if (length < GAME_RES_NAME_HEADER_SIZE) {
        return name;
    }
    return name - (GAME_RES_NAME_HEADER_SIZE - 1);
}

/* Best fit: the free block whose size equals size, else the smallest larger one; 0 when none. */
int GameResBlockFindBestFit(unsigned int size)
{
    unsigned int best_waste;
    int best;
    int index;

    best_waste = -1;
    best = 0;
    for (index = gameResBlockTable[0].owner; index < GAME_RES_BLOCK_COUNT && gameResBlockTable[index].buffer != 0; index++) {
        if (gameResBlockTable[index].owner == 0) {
            if (gameResBlockTable[index].size == size) {
                return index;
            }
            if (size < gameResBlockTable[index].size && gameResBlockTable[index].size - size < best_waste) {
                best_waste = gameResBlockTable[index].size - size;
                best = index;
            }
        }
    }
    return best;
}

/* The first block owned by owner whose buffer lies outside gameResArenaStart .. gameResArenaEnd. */
int GameResBlockFindByOwner(int owner)
{
    int index;

    if (owner == 0) {
        return 0;
    }
    for (index = 1; index < GAME_RES_BLOCK_COUNT && gameResBlockTable[index].buffer != 0; index++) {
        if ((gameResBlockTable[index].buffer < gameResArenaStart
             || gameResArenaEnd < gameResBlockTable[index].buffer)
            && gameResBlockTable[index].owner == owner) {
            return index;
        }
    }
    return 0;
}

/* The index of the block starting at buffer, or 0. */
int GameResBlockFindByBuffer(void *buffer)
{
    int index;

    if (buffer == 0) {
        return 0;
    }
    for (index = 1; index < GAME_RES_BLOCK_COUNT && gameResBlockTable[index].buffer != 0; index++) {
        if (gameResBlockTable[index].buffer == buffer) {
            return index;
        }
    }
    return 0;
}

/* The block outside gameResArenaStart .. gameResArenaEnd whose owner is a GAME_RES_OWNER tag and whose
 * first bytes hold the key gameResNameKey makes of name; 0 when none. GameResLoadName writes
 * that key as the header of the blocks it loads, but the test applies to every block
 * allocated without an owner, anonymous ones included. */
int GameResBlockFindByName(const char *name)
{
    const char *key;
    const char *wanted;
    char *stored;
    int index;

    key = gameResNameKey(name);
    for (index = 1; index < GAME_RES_BLOCK_COUNT && gameResBlockTable[index].buffer != 0; index++) {
        if ((gameResBlockTable[index].buffer < gameResArenaStart
             || gameResArenaEnd < gameResBlockTable[index].buffer)
            && (gameResBlockTable[index].owner & GAME_RES_OWNER_MASK) == GAME_RES_OWNER) {
            wanted = key;
            stored = gameResBlockTable[index].buffer;
            while (*wanted == *stored) {
                if (*wanted == 0) {
                    return index;
                }
                wanted++;
                stored++;
            }
        }
    }
    return 0;
}

/* Gives block index to owner, first splitting off the bytes beyond size as a new free block. */
void *GameResBlockAssign(int index, int owner, unsigned int size)
{
    GameResBlock *entry;
    int i;

    entry = &gameResBlockTable[index];
    if (entry->size != size) {
        for (i = GAME_RES_BLOCK_COUNT - 2; index < i; i--) {
            gameResBlockTable[i + 1].buffer = gameResBlockTable[i].buffer;
            gameResBlockTable[i + 1].size = gameResBlockTable[i].size;
            gameResBlockTable[i + 1].owner = gameResBlockTable[i].owner;
            gameResBlockTable[i + 1].tag = gameResBlockTable[i].tag;
            gameResBlockTable[i + 1].value = gameResBlockTable[i].value;
        }
        entry[1].size = entry->size - size;
        entry->size = size;
        entry[1].buffer = entry->buffer + size;
        entry[1].owner = 0;
    }
    entry->owner = owner;
    return entry->buffer;
}

/* Frees block index: it is marked free and absorbs the free block after it, then the free block
 * before it (index 2 and up). The entries past the merged block move down to close the gap. */
void GameResBlockFree(int index)
{
    GameResBlock *block;
    int dest;
    int srcIndex;

    block = &gameResBlockTable[index];
    block->owner = 0;
    dest = -1;
    srcIndex = -1;
    if (block[1].owner == 0) {
        block->size += block[1].size;
        dest = index + 1;
        srcIndex = index + 2;
    }
    if (index >= 2 && block[-1].owner == 0) {
        block[-1].size += block->size;
        dest = index;
        if (srcIndex == -1) {
            srcIndex = index + 1;
        }
    }
    if (srcIndex > 0) {
        if (srcIndex < GAME_RES_BLOCK_COUNT) {
            do {
                gameResBlockTable[dest].buffer = gameResBlockTable[srcIndex].buffer;
                gameResBlockTable[dest].size = gameResBlockTable[srcIndex].size;
                gameResBlockTable[dest].tag = gameResBlockTable[srcIndex].tag;
                gameResBlockTable[dest].owner = gameResBlockTable[srcIndex].owner;
                gameResBlockTable[dest].value = gameResBlockTable[srcIndex].value;
                srcIndex++;
                dest++;
            } while (srcIndex < GAME_RES_BLOCK_COUNT);
        }
    }
}

#include "shared.h"

static int xglCdEventState;

/* Completion handler passed to xglCdReadFile (proposed gameResReadEvent): event 1 stores value as the
 * state word xglCdEventState (xglCdEventState); event 4 calls the slot's callback with that state word. */
void gameResReadEvent(int event, int value, int slot) {
    GameResBlock *entry;
    GameResCallback callback;

    entry = &gameResBlockTable[slot];
    callback = (GameResCallback) entry->tag;
    if (callback != 0) {
        if (event == 1) {
            xglCdEventState = value;
        } else if (event == 4) {
            callback(slot, entry->buffer, xglCdEventState, entry->value);
        }
    }
}

extern int strcmp(const char *a, const char *b);
void GameResBlockFree(int index);

/* Returns the group whose id range holds file_id, or 0 when none does. */
CdFileGroup *xglCdFindFileGroup(unsigned int file_id)
{
    CdFileGroup *group;

    for (group = xglCdFileGroups; group->names != 0; group++) {
        if (file_id >= group->base && file_id < group->base + group->count) {
            return group;
        }
    }
    return 0;
}

/* The name of file_id within its group, or 0 when no group holds it. */
char *xglCdFindFileName(unsigned int file_id)
{
    CdFileGroup *group;

    group = xglCdFindFileGroup(file_id);
    if (group == 0) {
        return 0;
    }
    return group->names[file_id - group->base];
}

/* Returns the file id whose name is name, or -1 when no group lists it. */
int xglCdFindFileId(char *name)
{
    CdFileGroup *group;
    unsigned int index;

    for (group = xglCdFileGroups; group->names != 0; group++) {
        for (index = 0; index < group->count; index++) {
            if (strcmp(group->names[index], name) == 0) {
                return index + group->base;
            }
        }
    }
    return -1;
}

/* Writes the group's names[0] and then file_id's name to buffer; returns the end of the
 * written name (its NUL), or buffer unchanged when no group holds file_id. */
char *xglCdBuildFileName(char *buffer, unsigned int file_id)
{
    CdFileGroup *group;
    char **names;
    char *out;
    char *source;

    group = xglCdFindFileGroup(file_id);
    out = buffer;
    if (group != 0) {
        names = group->names;
        for (source = names[0]; *source != 0; source++) {
            *out++ = *source;
        }
        source = names[file_id - group->base];
        while ((*out++ = *source++) != 0) {
        }
        return out - 1;
    }
    return buffer;
}

/* Frees the block starting at buffer; returns its former index, 0 when no block starts there. */
int GameResBlockFreeByBuffer(void *buffer)
{
    int index;

    index = GameResBlockFindByBuffer(buffer);
    if (index == 0) {
        return 0;
    }
    GameResBlockFree(index);
    return index;
}

/* Frees the block of resource owner; returns its former index, 0 when none holds it. */
int GameResBlockFreeByOwner(int owner)
{
    int index;

    index = GameResBlockFindByOwner(owner);
    if (index == 0) {
        return 0;
    }
    GameResBlockFree(index);
    return index;
}

/* Allocates a block of size bytes (rounded up to GAME_RES_BLOCK_ALIGN) for owner; owner 0
 * makes a GAME_RES_OWNER tag from the block's address. Returns the block index, 0 when none fits. */
int GameResBlockAlloc(int owner, int size)
{
    int index;

    size = (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    index = GameResBlockFindBestFit(size);
    if (index == 0) {
        return 0;
    }
    if (owner == 0) {
        owner = GAME_RES_OWNER + (((unsigned int) gameResBlockTable[index].buffer >> 8) & 0xFFFF);
    }
    GameResBlockAssign(index, owner, size);
    return index;
}

/* Allocates a block of size bytes for no owner, clears its tag and value words and returns
 * its buffer, 0 when none fits. */
void *GameResBufferAlloc(int size)
{
    int index;

    index = GameResBlockAlloc(0, size);
    if (index != 0) {
        gameResBlockTable[index].tag = 0;
        gameResBlockTable[index].value = 0;
        return gameResBlockTable[index].buffer;
    }
    return 0;
}

/* The largest free block, 0 when none is free. */
int GameResBlockFindLargestFree(void)
{
    unsigned int best_size;
    int best;
    int index;

    best_size = 0;
    best = 0;
    for (index = 1; index < GAME_RES_BLOCK_COUNT && gameResBlockTable[index].buffer != 0; index++) {
        if (gameResBlockTable[index].owner == 0 && best_size < gameResBlockTable[index].size) {
            best_size = gameResBlockTable[index].size;
            best = index;
        }
    }
    return best;
}

/* Buffer and size of the largest free block (entry 0's when none is free). */
void *GameResourceGetFreeAddr(unsigned int *size)
{
    int index;

    index = GameResBlockFindLargestFree();
    if (size != 0) {
        *size = gameResBlockTable[index].size;
    }
    return gameResBlockTable[index].buffer;
}

/* Allocates a block of size bytes for owner, clears its tag and value words and returns
 * its buffer, 0 when none fits. */
void *GameResBufferAllocOwned(int owner, int size)
{
    int index;

    index = GameResBlockAlloc(owner, size);
    if (index != 0) {
        gameResBlockTable[index].tag = 0;
        gameResBlockTable[index].value = 0;
        return gameResBlockTable[index].buffer;
    }
    return 0;
}

/* GameResCallback (res_get_path.h) is called by GameResLoadID and GameResLoadName for a block that was already
 * loaded: block index, buffer, size and xglCdSlotValue. Stored as a block's read tag otherwise. */
static void *xglCdSlotValue;

/* Sets the value that xglCdReadFileTagged and xglCdReadFileSlot copy into the slot (xglCdSlotValue). */
void gameResSetReadValue(void *value)
{
    xglCdSlotValue = value;
}

int xglCdReadFileTagged(const char *name, void *buffer, int unused_arg2, GameResCallback tag) {
    int status;

    status = xglCdGetFileSize(name);
    if (status > 0) {
        if (buffer != 0) {
            gameResBlockTable[0].tag = tag;
            gameResBlockTable[0].value = xglCdSlotValue;
            xglCdSetReadTag(0);
            xglCdReadFile(name, buffer, 1, gameResReadEvent);
        }
    }
    return status;
}

/* Slot index to buffer (xglCdSlotGetBuffer): an index below 0x80 returns that entry's buffer; others pass through. */
unsigned int xglCdSlotGetBuffer(unsigned int index)
{
    unsigned int result;

    result = index;
    if (result != 0) {
        if (result < 0x80U) {
            result = (unsigned int) gameResBlockTable[result].buffer;
        }
    }
    return result;
}

static u8 gameResQueuedLoad;

/* Starts a read into a slot (proposed xglCdReadFileSlot). tag 0 reads without a slot;
 * otherwise the slot (index 0 when slot >= 0x80) takes tag and xglCdSlotValue before xglCdReadFile. */
void xglCdReadFileSlot(const char *name, void *buffer, GameResCallback tag, u32 slot) {
    GameResBlock *entry;
    u32 index;
    int in_table;
    void *value;

    in_table = slot < 0x80U;
    if (tag == 0) {
        if (gameResQueuedLoad != 0) {
            xglCdReadFile(name, buffer, 1, CD_NOTIFY_CURRENT);
            return;
        }
        xglCdReadFile(name, buffer, 0, CD_NOTIFY_CURRENT);
        return;
    }
    index = 0;
    entry = &gameResBlockTable[0];
    if (in_table != 0) {
        index = slot;
        entry = &gameResBlockTable[index];
    }
    value = xglCdSlotValue;
    entry->tag = tag;
    entry->value = value;
    xglCdSetReadTag(index);
    if (gameResQueuedLoad != 0) {
        xglCdReadFile(name, buffer, 1, gameResReadEvent);
        return;
    }
    xglCdReadFile(name, buffer, 0, gameResReadEvent);
}

/* Writes the extension ".xap" with its NUL at extension, the end of path, then probes path
 * (xglCdGetFileData): -1 when the .xap file is missing, 0 otherwise. */
int xglCdCheckXapFile(const char *path, char *extension)
{
    unsigned int record[0xC];

    extension[0] = '.';
    extension[1] = 'x';
    extension[2] = 'a';
    extension[3] = 'p';
    extension[4] = 0;
    return xglCdGetFileData(path, record);
}

/* Returns the name of the group that lists file_id, or 0 when no group does. */
char *xglCdFindXapName(int file_id)
{
    int *word;
    int groupName;
    int listedId;

    word = xglCdXapGroups;
    while (*word != -1) {
        groupName = *word;
        word++;
        listedId = *word++;
        while (listedId != -1) {
            if (listedId == file_id) {
                return (char *) groupName;
            }
            listedId = *word++;
        }
    }
    return 0;
}

/* File type of a file id (bits 16-23), as the loader dispatcher xglCdLoadByKind selects on it. */
#define CD_FILE_TYPE_MASK 0xFF0000
#define CD_FILE_TYPE(type) ((type) << 16)
/* File kinds: bits 16-23 of a file id, and the kinds of the entries resScriptRun collects in
 * resEntryTable (RES_KIND_SOUND .. RES_KIND_EVENT are entry kinds only, no file id has them). */
enum ResKind {
    RES_KIND_ACT = 0x10000,
    RES_KIND_MAP = 0x20000,
    RES_KIND_MOT = 0x30000,
    RES_KIND_EFFECT = 0x40000,
    RES_KIND_MOBJ = 0x70000,
    RES_KIND_ENE = 0x80000,
    RES_KIND_SOUND = 0x90000,
    RES_KIND_EVENT = 0xF10000
};
/* Event numbers of a RES_KIND_EVENT entry (bits 8-15): the "save" and "disk" commands. */
enum ResEvent {
    RES_EVENT_SAVE = 0x100,
    RES_EVENT_DISK = 0x200
};
/* Sound-bank numbers the script records name (seBankLoad kinds). */
#define SE_BANK_FOOT 7
#define SE_BANK_FOOT_MATERIAL 8
#define SE_BANK_ENEMY_FOOT 12
/* Footstep materials a scene lists (resFootMaterials holds at most this many). */
#define RES_FOOT_MATERIAL_MAX 9
/* Voice records of resRecordTable. */
#define SE_RECORD_COUNT 24
/* In a record's flags word: bit 31 = started with volume and pan, bits 0-30 = voice id. */
#define SE_RECORD_HAS_PARAMS 0x80000000
#define SE_VOICE_ID_MASK 0x7FFFFFFF
/* High bit of a resource id: the read is queued in the background (xglCdReadFile mode 1) and xglCdLoadByKind does not
   wait for the CD library to go idle. Without it the reads block until the data is in. */
#define RES_ID_QUEUED_LOAD 0x80000000
/* Owner flag of a block that resPreloadStep loaded for the next scene; resApplyCfFile compacts them. */
#define RES_OWNER_PRELOADED 0x40000000
/* Each player model slot is one block of this size followed by a RES_PATH_PAD_SIZE pad block. */
#define PLAYER_MODEL_BUFFER_SIZE 0xC5FC0
#define RES_PATH_PAD_SIZE 0x40
/* Read-completion event of xglCdReadFile; "#scn" is the script header magic. */
#define CD_EVENT_READ_DONE 4
#define RES_SCRIPT_MAGIC 0x6E637323
/* Jump records (resJumpRecords): kind of the end marker and of a record resJumpRecordSet filled. */
#define RES_JUMP_END 0xFF
#define RES_JUMP_NAMED 0x7F
/* Map block key class of a jump-point polygon; its low byte is the jump kind. */
#define RES_MAP_KEY_JUMP 0x100
/* cf file preload state: set while the file is being read. */
#define RES_CF_LOADING 0xFF
/* SB bytecode (OV01 VM) opcodes the jump scan reads, and the scratchpad address (0x70000000) the scan uses
 * as its private simulation stack, not the VM stack. */
#define SB_OP_END 0x1C
#define SB_OP_JUMP 0x0E
#define SB_OP_SWITCH 0x25
#define SB_OP_CALL_NATIVE 0x15
#define RES_SCAN_STACK_BASE 0x70000000

/* Appends the string src at dst and leaves dst on the copied NUL, so the next append continues
 * the string. Its do/while(0) block boundary keeps the dst-- ahead of the next call's argument
 * setup, as in the original. */
#define STRING_APPEND(dst, src) do { while ((*(dst)++ = *(src)++) != 0) {} (dst)--; } while (0)

/* Header of a file-set block (xglCdLoadFileSet): the data address of each file of the set,
 * 0 for a file that is missing. It takes the first 0x14 of GAME_RES_NAME_HEADER_SIZE bytes.
 * A map set (xglCdLoadBinFileSet) uses epf for its .id file, xap for the .bin script image and
 * xapBytes for that image's size. */
typedef struct CdFileSetHeader {
    char *pxy;
    char *txy;
    char *xhr;
    char *epf;
    char *xap;
    int xapBytes;
} CdFileSetHeader;

/* Returns the name of the group that lists file_id, or 0 when no group does. */ char *xglCdFindXapName(int file_id);
/* Default .xap name "man.xap" for ids whose own and fallback names are all missing
 * (res_get_path.c's own .sdata, still asm-owned data). */

extern char defaultXapName[8];

/* Loads the file set of file_id into slot's block: a CdFileSetHeader, then the .xap file and
 * the name's .epf, .xhr, .txy and .pxy files, each rounded up to GAME_RES_BLOCK_ALIGN and the
 * last to CD_SECTOR_SIZE. The .xap name is the group's override (xglCdFindXapName) or the file's
 * own name; when that file is missing, its trailing digits, then an 'H' before them, are
 * replaced by "00", and as the last resort (not for type 7) the default name "man.xap" is
 * appended to the group directory.
 * Returns the bytes the set takes. */
int xglCdLoadFileSet(unsigned int file_id, unsigned int slot, GameResCallback tag)
{
    char name[256];
    char path[256];
    char *extension;
    char *end;
    char *base;
    char *source;
    CdFileGroup *group;
    char **names;
    CdFileSetHeader *header;
    char *buffer;
    char *start;
    int size;

    start = buffer = (char *) xglCdSlotGetBuffer(slot);
    extension = xglCdBuildFileName(name, file_id);
    extension[0] = '.';
    /* end is reused: it first clears the header, then walks the .xap path (the original's
     * register use: one pseudo for both). */
    end = buffer;
    header = (CdFileSetHeader *) buffer;
    buffer += GAME_RES_NAME_HEADER_SIZE;
    if (slot != 0) {
        ((CdFileSetHeader *) end)->txy = 0;
        ((CdFileSetHeader *) end)->xhr = 0;
        ((CdFileSetHeader *) end)->epf = 0;
        ((CdFileSetHeader *) end)->xap = 0;
    }
    end = path;
    names = xglCdFindFileGroup(CD_FILE_TYPE(3))->names;
    for (source = names[0]; *source != 0; source++) {
        *end++ = *source;
    }
    base = end;
    group = xglCdFindFileGroup(file_id);
    names = group->names;
    source = names[file_id - group->base];
    STRING_APPEND(end, source);
    source = xglCdFindXapName(file_id);
    if (source != 0) {
        while ((*base++ = *source++) != 0) {
        }
        base[-1] = '.';
        base[0] = 'x';
        base[1] = 'a';
        base[2] = 'p';
        base[3] = 0;
    } else if (xglCdCheckXapFile(path, end) != 0) {
        /* strip the trailing decimal digits */
        while ((unsigned int) ((unsigned char) end[-1] - '0') < 10) {
            end--;
        }
        if (xglCdCheckXapFile(path, end) != 0) {
            if (end[-1] == 'H') {
                end--;
            }
            end[0] = '0';
            end[1] = '0';
            if (xglCdCheckXapFile(path, end + 2) != 0
                && (file_id & CD_FILE_TYPE_MASK) != CD_FILE_TYPE(7)) {
                for (source = defaultXapName; (*base++ = *source++) != 0;) {
                }
            }
        }
    }
    size = xglCdGetFileSize(path);
    if (size > 0) {
        if (slot != 0) {
            header->xap = buffer;
            xglCdReadFileSlot(path, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }
    extension[1] = 'e';
    extension[2] = 'p';
    extension[3] = 'f';
    extension[4] = 0;
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            header->epf = buffer;
            xglCdReadFileSlot(name, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }
    extension[1] = 'x';
    extension[2] = 'h';
    extension[3] = 'r';
    extension[4] = 0;
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            header->xhr = buffer;
            xglCdReadFileSlot(name, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }
    /* ".txy": the NUL written for ".xhr" stays */
    extension[1] = 't';
    extension[2] = 'x';
    extension[3] = 'y';
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            header->txy = buffer;
            xglCdReadFileSlot(name, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }
    /* ".pxy": read with the caller's tag and slot, no size check */
    extension[1] = 'p';
    size = xglCdGetFileSize(name);
    if (slot != 0) {
        header->pxy = buffer;
        xglCdReadFileSlot(name, buffer, tag, slot);
    }
    buffer += (size + (CD_SECTOR_SIZE - 1)) & ~(CD_SECTOR_SIZE - 1);
    return buffer - start;
}

void *memset(void *dest, int value, unsigned int count);

/* Loads the .bin, language-specific .sbl, .id, .txy and .pxy files of file_id into slot's
 * block (with slot 0 it only measures them) and returns the bytes the set takes. */
int xglCdLoadBinFileSet(unsigned int file_id, unsigned int slot, GameResCallback tag)
{
    char name[256];
    char *start;
    char *buffer;
    char *extension;
    char *dirEnd;
    char *sblPath;
    char *source;
    char *dest;
    char *binData;
    char *sblBuffer;
    CdFileSetHeader *header;
    CdFileSetHeader *info;
    int size;
    int sblSize;
    unsigned int binBytes;

    start = buffer = (char *) xglCdSlotGetBuffer(slot);
    extension = xglCdBuildFileName(name, file_id);
    header = (CdFileSetHeader *) buffer;
    info = (CdFileSetHeader *) buffer;
    buffer += GAME_RES_NAME_HEADER_SIZE;
    extension[0] = '.';
    if (slot != 0) {
        memset(header, 0, GAME_RES_NAME_HEADER_SIZE);
    }

    extension[4] = 0;
    extension[1] = 'b';
    extension[2] = 'i';
    extension[3] = 'n';
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0 && gameWork.resBuffer != 0) {
            if (gameResQueuedLoad != 0) {
                binData = buffer;
            } else {
                binData = (char *) gameWork.resBuffer;
            }
            header->xap = binData;
            xglCdReadFileSlot(name, binData, 0, 0);
        }
        binBytes = (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
        buffer += binBytes;
        sblBuffer = 0;
        if (slot != 0) {
            if (info->xap != 0) {
                sblBuffer = info->xap + binBytes;
            }
            info->xapBytes = binBytes;
        }

        sblPath = extension + 5;
        source = name;
        dest = sblPath;
        dirEnd = extension + 3;
        while (*dirEnd != '\\') {
            dirEnd--;
        }
        while (dirEnd >= source) {
            *dest++ = *source++;
        }
        xglGetLanguageCode(dest);
        dest[2] = '\\';
        dest = dest + 3;
        while (*source != '.') {
            *dest++ = *source++;
        }
        dest[0] = '.';
        dest[1] = 's';
        dest[2] = 'b';
        dest[3] = 'l';
        dest[4] = 0;
        sblSize = xglCdGetFileSize(sblPath);
        if (sblSize > 0) {
            if (slot != 0 && sblBuffer != 0) {
                sblBuffer = buffer;
                if (gameResQueuedLoad == 0) {
                    char *sblTop;

                    sblTop = (char *) ((u32) gameWork.resBuffer + info->xapBytes);
                    gameWork.imageEnd = (struct SbImageHeader *) sblTop;
                    sblBuffer = sblTop;
                }
                xglCdReadFileSlot(sblPath, sblBuffer, 0, 0);
            }
            buffer += (sblSize + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
        }
    }

    extension[1] = 'i';
    extension[2] = 'd';
    extension[3] = 0;
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            info->epf = buffer;
            xglCdReadFileSlot(name, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }

    extension[1] = 't';
    extension[2] = 'x';
    extension[3] = 'y';
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            info->txy = buffer;
            xglCdReadFileSlot(name, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }

    extension[1] = 'p';
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            info->pxy = buffer;
            xglCdReadFileSlot(name, buffer, tag, slot);
        }
        buffer += (size + (CD_SECTOR_SIZE - 1)) & ~(CD_SECTOR_SIZE - 1);
    }
    return buffer - start;
}

/* Reads file_id as ".xap" (or ".fap" when no ".xap" exists) into slot's buffer; returns the
 * bytes it takes rounded up to CD_SECTOR_SIZE, 0 when the file is missing. */
int xglCdLoadXapFile(unsigned int file_id, unsigned int slot, GameResCallback tag)
{
    char name[256];
    char *extension;
    char *buffer;
    char *start;
    int size;

    buffer = (char *) xglCdSlotGetBuffer(slot);
    start = buffer;
    extension = xglCdBuildFileName(name, file_id);
    extension[0] = '.';
    extension[1] = 'x';
    extension[2] = 'a';
    extension[3] = 'p';
    extension[4] = 0;
    size = xglCdGetFileSize(name);
    if (size < 0) {
        extension[1] = 'f';
        size = xglCdGetFileSize(name);
    }
    if (size > 0) {
        if (slot != 0) {
            xglCdReadFileSlot(name, buffer, tag, slot);
        }
        buffer += (size + (CD_SECTOR_SIZE - 1)) & ~(CD_SECTOR_SIZE - 1);
    }
    return buffer - start;
}

/* Same for ".ene" files, with no fallback extension. */
int xglCdLoadEneFile(unsigned int file_id, unsigned int slot, GameResCallback tag)
{
    char name[256];
    char *extension;
    char *buffer;
    char *start;
    int size;

    buffer = (char *) xglCdSlotGetBuffer(slot);
    start = buffer;
    extension = xglCdBuildFileName(name, file_id);
    extension[0] = '.';
    extension[1] = 'e';
    extension[2] = 'n';
    extension[3] = 'e';
    extension[4] = 0;
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            xglCdReadFileSlot(name, buffer, tag, slot);
        }
        buffer += (size + (CD_SECTOR_SIZE - 1)) & ~(CD_SECTOR_SIZE - 1);
    }
    return buffer - start;
}

/* target is the index of a block of gameResBlockTable or, from GAME_RES_BLOCK_COUNT up, the address to load to (0 only measures the set). */
int xglCdLoadByKind(int id, unsigned int target, int *size, GameResCallback tag);
/* Loads the .bin, language-specific .sbl, .id, .txy and .pxy files of file_id into slot's * block (with slot 0 it only measures them) and returns the bytes the set takes. */ int xglCdLoadBinFileSet(unsigned int file_id, unsigned int slot, GameResCallback tag);

int xglCdLoadByKind(int resourceId, unsigned int target, int *outSize, GameResCallback tag)
{
    unsigned int fileId;
    unsigned int type;
    int bytes;

    fileId = resourceId & GAME_RES_ID_MASK;
    if (resourceId < 0) {
        gameResQueuedLoad = 1;
    } else {
        gameResQueuedLoad = 0;
    }
    bytes = 0;
    if (gameResQueuedLoad == 1 || xglCdServiceIfPending() == 0) {
        if (xglCdFindFileGroup(fileId) != 0) {
            type = fileId & CD_FILE_TYPE_MASK;
            switch (type) {
            case RES_KIND_ACT:
            case RES_KIND_MOBJ:
                bytes = xglCdLoadFileSet(fileId, target, tag);
                break;
            case RES_KIND_MAP:
                bytes = xglCdLoadBinFileSet(fileId, target, tag);
                break;
            case RES_KIND_MOT:
                bytes = xglCdLoadXapFile(fileId, target, tag);
                break;
            case RES_KIND_EFFECT:
                break;
            case RES_KIND_ENE:
                bytes = xglCdLoadEneFile(fileId, target, tag);
                break;
            }
        }
    }
    if (outSize != 0) {
        *outSize = bytes;
    }
    return bytes;
}

/* Loads the file name into a block whose GAME_RES_NAME_HEADER_SIZE header holds the name, unless a block
 * already holds it. Reports the data (past the header) and its size; returns the owner tag,
 * -1 when the file is missing or no block is free. */
int GameResLoadName(const char *name, void **data, unsigned int *size, GameResCallback callback)
{
    int index;
    int bytes;
    char *buffer;
    const char *source;
    char *header;

    index = GameResBlockFindByName(name);
    if (index > 0) {
        buffer = gameResBlockTable[index].buffer;
        buffer += GAME_RES_NAME_HEADER_SIZE;
        if (data != 0) {
            *data = buffer;
        }
        if (size != 0) {
            *size = gameResBlockTable[index].size;
        }
        if (callback != 0) {
            callback(index, gameResBlockTable[index].buffer, gameResBlockTable[index].size, xglCdSlotValue);
        }
        return gameResBlockTable[index].owner;
    }
    bytes = xglCdGetFileSize(name);
    if (bytes < 0) {
        return -1;
    }
    bytes = (bytes + (CD_SECTOR_SIZE - 1)) & ~(CD_SECTOR_SIZE - 1);
    bytes += GAME_RES_NAME_HEADER_SIZE;
    index = GameResBlockAlloc(0, bytes);
    if (index == 0) {
        return -1;
    }
    buffer = gameResBlockTable[index].buffer;
    source = gameResNameKey(name);
    header = buffer;
    while ((*header++ = *source++) != 0) {
    }
    gameResBlockTable[index].tag = callback;
    gameResBlockTable[index].value = xglCdSlotValue;
    buffer += GAME_RES_NAME_HEADER_SIZE;
    if (data != 0) {
        *data = buffer;
    }
    if (size != 0) {
        *size = bytes;
    }
    xglCdSetReadTag(index);
    xglCdReadFile(name, buffer, 1, gameResReadEvent);
    return gameResBlockTable[index].owner;
}

/* Returns the owner tag of the block holding resource id, loading it first when no block
 * holds it (-1: no such file, -2: no free block). Reports buffer and size, and calls
 * callback with the block when it was already loaded. */
int GameResLoadID(int id, void **buffer, unsigned int *size, GameResCallback callback)
{
    unsigned int file_id;
    int index;
    int total;

    file_id = id & GAME_RES_ID_MASK;
    index = GameResBlockFindByOwner(file_id);
    if (index != 0) {
        if (buffer != 0) {
            *buffer = gameResBlockTable[index].buffer;
        }
        if (size != 0) {
            *size = gameResBlockTable[index].size;
        }
        if (callback != 0) {
            callback(index, gameResBlockTable[index].buffer, gameResBlockTable[index].size, xglCdSlotValue);
        }
        return gameResBlockTable[index].owner;
    }
    total = xglCdLoadByKind(id, 0, 0, 0);
    if (total == 0) {
        return -1;
    }
    index = GameResBlockAlloc(file_id, total);
    if (index == 0) {
        return -2;
    }
    xglCdLoadByKind(id, index, size, callback);
    if (buffer != 0) {
        *buffer = gameResBlockTable[index].buffer;
    }
    return gameResBlockTable[index].owner;
}

/* The data (past the GAME_RES_NAME_HEADER_SIZE header) of the block loaded by name, and its size; 0 when absent. */
void *GameResFindName(char *name, unsigned int *size)
{
    int index;
    char *data;

    index = GameResBlockFindByName(name);
    if (index > 0) {
        data = gameResBlockTable[index].buffer;
        data += GAME_RES_NAME_HEADER_SIZE;
        if (size != 0) {
            *size = gameResBlockTable[index].size;
        }
        return data;
    }
    return 0;
}

#include "main/se_bank_load.h"
#include "main/sound_driver.h"

int GameResBlockFindByOwner(int owner);

/* Entry 0 of gameResBlockTable (GameResBlock above) is the header: GameResourceInit stores the
   managed range start in its buffer and length in its size, and its owner holds the matched index. */

void *GameResFindID(int resourceId, int *outSize)
{
    int block;

    block = GameResBlockFindByOwner(resourceId & GAME_RES_ID_MASK);
    if (block == 0) {
        return 0;
    }
    if (outSize != 0) {
        *outSize = gameResBlockTable[block].size;
    }
    return gameResBlockTable[block].buffer;
}

int GameResBlockFindLargestFree(void);

/* Stores the base block index of the block searches in gameResBlockTable[0].owner (GameResourceReset starts from it too):
   the largest free block (GameResBlockFindLargestFree) when search is nonzero, otherwise block 1. */
void resSelectSlot(int search) {
    if (search != 0) {
        gameResBlockTable[0].owner = GameResBlockFindLargestFree();
    } else {
        gameResBlockTable[0].owner = 1;
    }
}

/* Name of the sound bank loaded into each of the 32 slots; slots 3 and 4 hold the map banks. */
static char *seBankNames[32];
static char *seBankNameCursor;
static char seBankNamePool[256];
static signed char resFootMaterials[16];
static int resEnemyFootGroups[16];

void resResetDescriptorTables(void) {
    int i;

    for (i = 0; i < 32; i++) {
        seBankNames[i] = 0;
    }
    seBankNames[2] = cfsSeBankNamePointer;
    seBankNameCursor = seBankNamePool;
    for (i = 0; i < RES_FOOT_MATERIAL_MAX; i++) {
        resFootMaterials[i] = -1;
    }
    for (i = 0; i < 16; i++) {
        resEnemyFootGroups[i] = -1;
    }
}

/* Another view of the 16-byte voice record of resRecordTable (see ResRecord and SeVoiceEntry); tag is word 0,
 * whose top byte (bits 24..30) is the bank slot that resReleaseRecordsOfType compares. */
typedef struct {
    u32 kindWord;
    u8 unmodeled_04[0xC];
} ResKindRecord;

/* One 16-byte entry of resRecordTable (0x6A12E0): a voice's start words and its key/flags word. */
/* In a record whose flags have SE_RECORD_HAS_PARAMS, volume and pan are what seVoiceStart and
 * seVoiceSetPanVolume take (seVoiceRecordsUpdatePan passes best >> 8 and best & 0x7F); without it the first
 * three words are the emitter position (seVoiceRecordAddAt). */
typedef struct ResRecord {
    union {
        struct {
            u32 volume;
            u32 pan;
            u32 unmodeled_08;
        } params;
        f32 position[3];
    } u;
    u32 flags;
} ResRecord;

static ResRecord resRecordTable[24];
static unsigned char mapSeBankNameBuffer[16];
static unsigned char mapSeSecondBankNameBuffer[16];
static signed char seBgmName[32];

/* Clears the type word of all 24 records and the three flags at seBgmName, mapSeBankNameBuffer and mapSeSecondBankNameBuffer (proposed resClearRecordTable body). */
void resClearRecordTable(void)
{
    int i;

    seBgmName[0] = 0;
    mapSeBankNameBuffer[0] = 0;
    mapSeSecondBankNameBuffer[0] = 0;
    for (i = SE_RECORD_COUNT - 1; i >= 0; i--) {
        resRecordTable[i].flags = 0;
    }
}

/* 16-byte voice entries of resRecordTable (0x6A12E0); the flags word is tested and its top bit selects the start path. */
typedef struct SeVoiceEntry {
    int startA;
    int startB;
    u8 unmodeled_08[4];
    unsigned int flags;
} SeVoiceEntry;

extern int seBankAddressTable[];

void seStateRestore(int mode) {
    int i;
    int j;
    u32 flags;
    int voice;

    seBankLoadDefault(0, 0);
    for (i = 2; i < SE_RECORD_COUNT; i++) {
        seBankLoadDefault(i, 0);
    }
    for (i = 0; i < 32; i++) {
        if (seBankNames[i] != 0) {
            seBankLoad(i, seBankNames[i], seBankAddressTable[i]);
        }
    }
    seFootBanksLoad();
    if (mode < 0) {
        seBgmName[0] = 0;
        return;
    }
    if (seBgmName[0] != 0) {
        seBgmPlay(mode + 1, seBgmName, 0x19000);
    }
    for (i = 0; i < SE_RECORD_COUNT; i++) {
        flags = resRecordTable[i].flags;
        if (flags != 0) {
            voice = flags & SE_VOICE_ID_MASK;
            if (flags & SE_RECORD_HAS_PARAMS) {
                seVoiceStart(voice, resRecordTable[i].u.params.volume, resRecordTable[i].u.params.pan);
            } else {
                for (j = 0; j < i; j++) {
                    if (resRecordTable[i].flags == resRecordTable[j].flags) {
                        j = -1;
                        break;
                    }
                }
                if (j != -1) {
                    seVoiceStart(voice, 0, 64);
                }
            }
        }
    }
}

void seStateRestore(int mode);

void seStateRestoreField(void) {
    seStateRestore(0x10000);
}

/* Clears the type word of all 24 records and the three flags at seBgmName, mapSeBankNameBuffer and mapSeSecondBankNameBuffer (proposed resClearRecordTable body). */ void resClearRecordTable(void);

void seStateClear(void) {
    seStateRestore(-1);
    resClearRecordTable();
}

void seStateRestoreDefault(void) {
    seStateRestore(0);
}

typedef struct {
    unsigned char b;
} ResByte;

int seNameCopyIfChanged(char *dst, char *src)
{
    signed char srcChar;
    ResByte srcByte;
    int changed;

    srcChar = *src;
    srcByte.b = *src;
    changed = 1;
    if (srcChar != *dst) {
        *dst = srcByte.b;
        if (srcChar != 0) {
            do {
                src++;
                dst++;
                *dst = *src;
            } while (((unsigned int)*dst << 24) != 0);
        }
    } else {
        if (((unsigned int)srcByte.b << 24) == 0) {
            return 0;
        }
        src++;
        dst++;
        return seNameCopyIfChanged(dst, src);
    }
    return changed;
}

#include "main/game_camera.h"

void resReleaseRecordsOfType(int kind, int forward) {
    int i;
    u8 *base;
    ResKindRecord *record;

    if (forward == 0) {
        base = (u8 *)resRecordTable;
        record = (ResKindRecord *)(base + 12);
        for (i = 0; i < SE_RECORD_COUNT; i++, record++) {
            if (((record->kindWord >> 24) & 0x7f) == kind) {
                seVoiceStop(record->kindWord);
                record->kindWord = 0;
            }
        }
    } else {
        base = (u8 *)resRecordTable;
        record = (ResKindRecord *)(base + 12);
        for (i = SE_RECORD_COUNT - 1; i >= 0; i--, record++) {
            if ((record->kindWord >> 24) == kind) {
                seVoiceStop(record->kindWord);
                record->kindWord = 0;
            }
        }
    }
    soundInitIfNeeded();
}

void resReleaseRecordsOfType(int kind, int forward);
int seNameCopyIfChanged(char *dst, char *src);

int seBankSetName(int kind, char *text)
{
    char *cursor;
    char *name;
    char *source;
    u32 textAddress;
    int bankSize;

    textAddress = (u32)text + (((u32)text & 0xFF) >> 8);
    name = text;
    if (name != 0) {
        switch (kind) {
        case 3:
            cursor = mapSeBankNameBuffer;
            seBankNames[3] = cursor;
            resReleaseRecordsOfType(kind, 1);
            if (seNameCopyIfChanged(cursor, name) == 0) {
                return 0;
            }
            resReleaseRecordsOfType(kind, 0);
            break;
        case 4:
            cursor = mapSeSecondBankNameBuffer;
            seBankNames[4] = cursor;
            resReleaseRecordsOfType(kind, 1);
            if (seNameCopyIfChanged(cursor, name) == 0) {
                return 0;
            }
            resReleaseRecordsOfType(kind, 0);
            break;
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
            break;
        default:
            cursor = seBankNameCursor;
            seBankNames[kind] = cursor;
            source = (char *)textAddress;
            while ((*cursor++ = *source++) != 0) {
            }
            seBankNameCursor = cursor;
            break;
        }
    } else {
        switch (kind) {
        case 3:
            mapSeBankNameBuffer[0] = 0;
            resReleaseRecordsOfType(kind, (int)textAddress);
            /* fall through */
        case 4:
            mapSeSecondBankNameBuffer[0] = 0;
            resReleaseRecordsOfType(kind, 0);
            break;
        }
    }
    if (kind == 3) {
        seBankLoad(4, 0, 0);
        bankSize = seBankLoad(3, name, seBankAddressTable[3]);
        seBankAddressTable[4] = 0xBE000 + ((bankSize + 255) & -256);
        return bankSize;
    }
    return seBankLoad(kind, name, seBankAddressTable[kind]);
}

void korgEvagPauseSeq(int, int);

void soundRestart(void) {
    soundSlotsRestart();
    korgEvagPauseSeq(1, 1);
    soundDriverReset();
}

void soundReset(void) {
    soundRestart();
    seSetMuted(1, 0);
    seBgmStop();
}

static signed char seBgmVolume;

void seBgmNameSet(char *name)
{
    if (*name == 0) {
        seBgmPlay(1, 0, 0x19000);
        seBgmStop();
        seBgmName[0] = 0;
        return;
    }
    if (seNameCopyIfChanged(seBgmName, name) != 0) {
        seVolumeSet(seBgmVolume, 0);
        seBgmPlay(0x20001, seBgmName, 0x19000);
    } else {
        seVolumeSet(seBgmVolume, 16);
    }
}

void seVoiceRecordSet(u32 voice, u32 volume, u32 pan) {
    int freeSlot = -1;
    int i;
    for (i = 0; i < SE_RECORD_COUNT; i++) {
        if (resRecordTable[i].flags == 0 && freeSlot == -1) {
            freeSlot = i;
        }
        if ((resRecordTable[i].flags & SE_VOICE_ID_MASK) == voice) {
            if (volume != 0) {
                resRecordTable[i].flags = voice | SE_RECORD_HAS_PARAMS;
                resRecordTable[i].u.params.volume = volume;
                resRecordTable[i].u.params.pan = pan;
                seVoiceSetPanVolume(voice, volume, pan);
                return;
            }
            /* With volume 0 (stop), pan is read as a record index. */
            if (!(resRecordTable[i].flags & SE_RECORD_HAS_PARAMS) && (resRecordTable[pan].flags & SE_VOICE_ID_MASK) == voice) {
                resRecordTable[pan].flags = 0;
                for (i = 0; i < SE_RECORD_COUNT; i++) {
                    if ((resRecordTable[i].flags & SE_VOICE_ID_MASK) == voice) {
                        return;
                    }
                }
                seVoiceStop(voice);
                return;
            }
            resRecordTable[i].flags = 0;
            seVoiceStop(voice);
            return;
        }
    }
    if (volume != 0) {
        resRecordTable[freeSlot].flags = voice | SE_RECORD_HAS_PARAMS;
        resRecordTable[freeSlot].u.params.volume = volume;
        resRecordTable[freeSlot].u.params.pan = pan;
        seVoiceStart(voice, volume, pan);
    }
}

static void *gameResWorkStart;
static void *gameResWorkEnd;

typedef union {
    u32 w;
} ResTagWord;

void seVoiceRecordAddAt(int voice, f32 *src)
{
    int i;
    int freeSlot = -1;
    int running = 0;

    for (i = 0; i < SE_RECORD_COUNT; i++) {
        u32 flagsWord = resRecordTable[i].flags;

        if (flagsWord == 0 && freeSlot == -1) {
            freeSlot = i;
        }
        if ((flagsWord & SE_VOICE_ID_MASK) == voice) {
            running = 1;
        }
    }
    {
        f32 *slotPos = resRecordTable[freeSlot].u.position;

        ((ResTagWord *)&resRecordTable[freeSlot].flags)->w = voice;
        slotPos[0] = src[0];
        ((f32 *)&resRecordTable[freeSlot].u.params.pan)[0] = src[1];
        ((f32 *)&resRecordTable[freeSlot].u.params.pan)[1] = src[2];
    }
    if (!running) {
        seVoiceStart(voice, 0, 64);
    }
}


void seVoiceRecordsUpdatePan(void)
{
    int i;
    int j;
    u32 loudest;

    if (gameWork.control & CONTROL_MAP_LOADED) {
        return;
    }
    for (i = 0; i < SE_RECORD_COUNT; i++) {
        if (resRecordTable[i].flags == 0 || (resRecordTable[i].flags & SE_RECORD_HAS_PARAMS)) {
            continue;
        }
        for (j = 0; j < i; j++) {
            if (resRecordTable[i].flags == resRecordTable[j].flags) {
                j = -1;
                break;
            }
        }
        if (j == -1) {
            continue;
        }
        loudest = seVoicePanVolumeFromPosition(resRecordTable[i].u.position);
        for (j = i + 1; j < SE_RECORD_COUNT; j++) {
            if (resRecordTable[i].flags == resRecordTable[j].flags) {
                u32 candidate = seVoicePanVolumeFromPosition(resRecordTable[j].u.position);
                if (candidate > loudest) {
                    loudest = candidate;
                }
            }
        }
        seVoiceSetPanVolume(resRecordTable[i].flags, loudest >> 8, loudest & 0x7F);
    }
}

static f32 seMaxDistance;

/* Sets the sound's maximum audible distance when maxDistance is not negative; returns the current value
   (proposed seSetMaxDistance). The distance check in seVoicePanVolumeFromPosition reads seMaxDistance. */
f32 seSetMaxDistance(f32 maxDistance) {
    if (maxDistance >= 0.0f) {
        seMaxDistance = maxDistance;
    }
    return seMaxDistance;
}

extern int uwamonoCurrentIndex;

u32 seVoicePanVolumeFromPosition(f32 *position)
{
    struct XenoActor *emitter;
    f32 *target;
    Vector4 offset;
    f32 distance;
    f32 angle;
    int volume;
    int pan;

    emitter = uwamonoRecords[uwamonoCurrentIndex].body.actor;
    if (emitter == 0) {
        return 0;
    }
    target = &emitter->position.x;
    offset.x = position[0] - target[0];
    offset.y = position[1] - target[1];
    offset.z = position[2] - target[2];
    xglVectorLength(&distance, &offset);
    if (seMaxDistance < distance) {
        return 0;
    }
    volume = F2I((1.0f - distance / seMaxDistance) * 127.0f);
    angle = xglAtan2(offset.x, offset.z) + gameWork.camera.range.northAngle + 1.5707963705062866f;
    while (6.2831854820251465f < angle) {
        angle -= 6.2831854820251465f;
    }
    while (angle < 0.0f) {
        angle += 6.2831854820251465f;
    }
    if (3.1415927410125732f < angle) {
        angle = 6.2831854820251465f - angle;
    }
    pan = F2I(angle / 3.1415927410125732f * 127.0f);
    return (volume << 8) + pan;
}

void *GameResourceWorkAlloc(unsigned int bytesWanted)
{
    unsigned int first;
    unsigned int last;
    unsigned int total;

    first = GameResBlockFindLargestFree();
    last = first;
    total = gameResBlockTable[first].size;
    if (total < bytesWanted) {
        while (total < bytesWanted) {
            if (gameResBlockTable[last + 1].buffer == 0 && gameResBlockTable[0].owner < first) {
                first--;
                total += gameResBlockTable[first].size;
            } else {
                last++;
                total += gameResBlockTable[last].size;
            }
        }
    }
    if (gameResArenaEnd == 0) {
        gameResArenaStart = gameResBlockTable[first].buffer;
        gameResArenaEnd = gameResBlockTable[last].buffer;
    } else {
        gameResWorkStart = gameResBlockTable[first].buffer;
        gameResWorkEnd = gameResBlockTable[last].buffer;
    }
    return gameResBlockTable[first].buffer;
}

void GameResourceWorkReload(void)
{
    int i;
    GameResBlock *entry;
    GameResBlock *first;
    GameResBlock *table;
    void *blockStart;

    table = gameResBlockTable;
    first = &table[1];
    i = 1;
    if (first->buffer != 0) {
        entry = first;
        do {
            if (entry->owner != 0) {
                blockStart = entry->buffer;
                if ((blockStart < gameResWorkStart || blockStart > gameResWorkEnd) && blockStart >= gameResArenaStart && blockStart <= gameResArenaEnd) {
                    xglCdLoadByKind(entry->owner | RES_ID_QUEUED_LOAD, (unsigned int) blockStart, 0, 0);
                }
            }
            i++;
            entry++;
        } while (i < 0x80 && entry->buffer != 0);
    }
}

void GameResArenaRelease(void)
{
    int i;
    GameResBlock *entry;
    GameResBlock *first;
    GameResBlock *table;
    void *blockStart;
    void *lower;
    void *upper;

    if (gameResWorkEnd == 0) {
        lower = gameResArenaStart;
        upper = gameResArenaEnd;
    } else {
        lower = gameResWorkStart;
        upper = gameResWorkEnd;
    }
    table = gameResBlockTable;
    first = &table[1];
    i = 1;
    if (first->buffer != 0) {
        entry = first;
        do {
            if (entry->owner != 0) {
                blockStart = entry->buffer;
                if (blockStart >= lower && blockStart <= upper) {
                    xglCdLoadByKind(entry->owner, (unsigned int) blockStart, 0, 0);
                }
            }
            i++;
            entry++;
        } while (i < 0x80 && entry->buffer != 0);
    }
    gameResArenaStart = 0;
    gameResArenaEnd = 0;
    gameResWorkStart = 0;
    gameResWorkEnd = 0;
}

void cdSlotTestOnStart(void) {
}

/* One of the four player model slots: the start of its buffer block and the resource id loaded into it last
 * (resPlayerSlotsLoadModels' model ids; 0 for none). */
typedef struct PlayerModelSlot {
    u8 *base;
    int resource_id;
} PlayerModelSlot;

static PlayerModelSlot resPlayerModelSlots[4];

void GameResourceReset(void) {
    int i = gameResBlockTable[0].owner;

    if (i == 1) {
        gameResBlockTable[1].buffer = gameResBlockTable[0].buffer;
    }
    gameResBlockTable[i].size = gameResBlockTable[0].size - (gameResBlockTable[i].buffer - gameResBlockTable[0].buffer);
    gameResBlockTable[i].owner = 0;
    for (i++; i < 128; i++) {
        gameResBlockTable[i].buffer = 0;
        gameResBlockTable[i].size = 0;
        gameResBlockTable[i].owner = -1;
    }
    gameResArenaStart = 0;
    gameResArenaEnd = 0;
    gameResWorkStart = 0;
    gameResWorkEnd = 0;
}


/* Stores the managed range in the header entry, then resets the path state (proposed GameResourceInit). */
void GameResourceInit(void *base, unsigned int size) {
    gameResBlockTable[0].buffer = base;
    gameResBlockTable[0].size = size;
    resSelectSlot(0);
    GameResourceReset();
    resResetDescriptorTables();
    resClearRecordTable();
    bgmNameListIndex = 0;
}

u8 *next_line(u8 *cursor)
{
    for (;;) {
        while (*cursor != '\n' && *cursor != '\r')
            cursor++;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')
            cursor++;
        if (*cursor != ';')
            return cursor;
    }
}

u8 *skip_space(u8 *cursor)
{
    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    return cursor;
}

u8 *resParseFileGroup(u8 *text, int kind, int *out)
{
    CdFileGroup *group;
    char **names;
    unsigned int nameIndex;
    u8 *cursor;
    u8 *groupName;
    int textChar;
    int nameChar;

    for (group = xglCdFileGroups; group->base != -1; group++) {
        if (kind != 0 && (group->base & CD_FILE_TYPE_MASK) != kind) {
            continue;
        }
        names = group->names;
        for (nameIndex = 1, names++; nameIndex < group->count; nameIndex++, names++) {
            groupName = (u8 *)*names;
            cursor = text;
            for (;;) {
                for (;;) {
                    textChar = *cursor++;
                    if (textChar == ',' || textChar == ' ' || textChar == '\t' || textChar == '\n' || textChar == '\r' || textChar == 0) {
                        textChar = 0;
                        break;
                    }
                    if (textChar != '_') {
                        textChar |= 0x20;
                        break;
                    }
                }
                for (;;) {
                    nameChar = *groupName++;
                    if (nameChar == ' ' || nameChar == '\t' || nameChar == '\n' || nameChar == '\r' || nameChar == 0) {
                        nameChar = 0;
                        break;
                    }
                    if (nameChar != '_') {
                        nameChar |= 0x20;
                        break;
                    }
                }
                if ((textChar | nameChar) == 0) {
                    if (out != 0) {
                        *out = group->base + nameIndex;
                    }
                    return cursor;
                }
                if (textChar != nameChar) {
                    break;
                }
            }
        }
    }
    if (out != 0) {
        *out = -1;
    }
    return text;
}

static u32 resEntryTable[64];
static char resNamePool[1024];
static char *resNameCursor;

/*
 * Appends one word to resEntryTable; word 0 holds the next free index. Keys 0x40000, 0x90000 and 0xF00000
 * store key plus the offset of resNameCursor in resNamePool, then copy the NUL-terminated name at name
 * (when name is not null) to the cursor. Key 0xF10000 stores the integer name plus key, and other keys store key.
 * The original passes the number of a RES_KIND_EVENT entry in this pointer parameter (the callers then
 * warn); typing it u32 changes the generated code.
 */
void resEntryAdd(u32 key, u8 *name)
{
    switch (key) {
    case RES_KIND_EFFECT:
    case RES_KIND_SOUND:
    case GAME_RES_OWNER:
        {
            int index;

            index = resEntryTable[0];
            resEntryTable[index] = key + (resNameCursor - resNamePool);
            resEntryTable[0] = index + 1;
            if (name != 0) {
                while ((*resNameCursor++ = *name++) != 0) {
                }
            }
        }
        break;
    case RES_KIND_EVENT:
        {
            int index;

            index = resEntryTable[0];
            resEntryTable[index] = (int) name + key;
            resEntryTable[0] = index + 1;
        }
        break;
    default:
        {
            int index;

            index = resEntryTable[0];
            resEntryTable[index] = key;
            resEntryTable[0] = index + 1;
        }
        break;
    }
}

u8 *resParseFileGroup(u8 *text, int kind, int *out);

/* Parses a name at cursor for kind and records the match; returns the cursor (proposed resCmdResolve). */
u8 *resCmdResolve(u8 *cursor, int kind) {
    int entryId;

    resParseFileGroup(cursor, kind, &entryId);
    resEntryAdd(entryId, 0);
    return cursor;
}

/* Script command handlers; each passes its kind to resCmdResolve (act: 0x10000, map: 0x20000, mot: 0x30000, mobj: 0x70000). */
u8 *resCmdAct(u8 *cursor) {
    return resCmdResolve(cursor, 0x10000);
}

u8 *resCmdMap(u8 *cursor) {
    return resCmdResolve(cursor, 0x20000);
}

u8 *resCmdMot(u8 *cursor) {
    return resCmdResolve(cursor, 0x30000);
}

u8 *resCmdMobj(u8 *cursor) {
    return resCmdResolve(cursor, 0x70000);
}

/* Copies the name at cursor into the name buffer up to whitespace and returns the cursor after it. */
u8 *resCmdEffect(u8 *cursor) {
    int c;

    resEntryAdd(RES_KIND_EFFECT, 0);
    c = *cursor;
    if ((u8)c == 0x20 || (u8)c == 0x9 || (u8)c == 0xA || (u8)c == 0xD) {
    } else {
        do {
            cursor++;
            *resNameCursor++ = c;
            c = *cursor;
            if ((u8)c == 0x20 || (u8)c == 0x9 || (u8)c == 0xA) {
                break;
            }
        } while ((u8)c != 0xD);
    }
    *resNameCursor++ = 0;
    return cursor;
}

u8 *resCmdMapse(u8 *script)
{
    resEntryAdd(RES_KIND_SOUND, 0);
    *resNameCursor++ = 3;
    while (*script != ',' && *script != ' ' && *script != '\t' && *script != '\n' && *script != '\r') {
        *resNameCursor++ = *script++;
    }
    *resNameCursor++ = 0;
    resEntryAdd(RES_KIND_SOUND, 0);
    *resNameCursor++ = 4;
    if (*script++ == ',') {
        while (*script != ',' && *script != ' ' && *script != '\t' && *script != '\n' && *script != '\r') {
            *resNameCursor++ = *script++;
        }
    }
    *resNameCursor++ = 0;
    return script;
}

/*
 * Script command handler for the runtime BGM name list: adds the 0x90000 entry, writes the prefix 's' to the
 * name pool, then copies the names at script, splitting at ',' (each name NUL-terminated), until a space,
 * tab, line feed or carriage return. Writes a final NUL and returns the pointer where the copy stopped.
 */
u8 *resCmdRuntimeBgm(u8 *script)
{
    int c;
    u8 ch;

    resEntryAdd(RES_KIND_SOUND, 0);
    *resNameCursor++ = 's';
    ch = *script;
    /* ch + ((ch & 0xFF00) >> 8) is ch; only combine folds the zero offset, so the byte copy survives cprop. */
    c = (int) ch + (((u32) ch & 0xFF00) >> 8);
    while ((u8) c != ' ' && (u8) c != '\t' && (u8) c != '\n' && (u8) c != '\r') {
        if (c == ',') {
            *resNameCursor++ = '\0';
            script++;
            ch = *script;
        }
        script++;
        *resNameCursor++ = ch;
        c = *script;
        /* the same zero-offset copy keeps the byte that the next pass stores alive */
        ch = (u8) (c + (((u32) c & 0xFF) >> 8));
    }
    *resNameCursor++ = '\0';
    return script;
}

u8 *resCmdBgmVol(u8 *cursor) {
    int volume = 0;
    char **cursorSlot;
    char *writePos;
    char *next;

    if ((u8)(*cursor - '0') < 10) {
        do {
            int times5 = volume * 5;
            volume = times5 << 1;
            volume = (volume + *cursor) - '0';
            cursor++;
        } while ((u8)(*cursor - '0') < 10);
    }
    resEntryAdd(RES_KIND_SOUND, 0);
    cursorSlot = &resNameCursor;
    writePos = *cursorSlot;
    *writePos = 'v';
    writePos++;
    *cursorSlot = writePos;
    next = writePos + 1;
    *writePos = (u8) volume;
    *cursorSlot = next;
    return cursor;
}

static signed char footMaterialList[16];
u8 *resCmdFoot(u8 *script) {
    u8 *materialList = footMaterialList;
    u8 *materialListStart = materialList;
    int material;

    for (;;) {
        material = footMaterialFind(script);
        script += 3;
        resEntryAdd(RES_KIND_SOUND, 0);
        resNameCursor[0] = 'f';
        resNameCursor[1] = materialList - materialListStart;
        resNameCursor[2] = material;
        *materialList++ = material;
        resNameCursor += 3;
        if (*script == '\n' || *script == '\r') {
            *materialList = 0xFF;
            return script;
        }
        for (;;) {
            if (*script == ',' || *script == ' ' || *script == '\t') {
                script++;
            } else {
                break;
            }
        }
    }
}

void *footMaterialKey(int index);

/* Parses the script's word list: one 'e' record per word, then one record per footstep material (proposed resCmdEnefoot). */
u8 *resCmdEnefoot(u8 *script)
{
    u8 footName[20] = "fs_0123456789abcdef";
    u8 *nameEnd;
    u8 *copyPos;
    u8 *materialKey;
    int groupId;
    u8 groupWord[4];
    int groupIndex;
    int recordCode = 12;
    int materialIndex;

    groupIndex = 0;
    for (;;) {
        nameEnd = footName + 3;
        for (;;) {
            if (*script == ',' || *script == ' ' || *script == '\t' || *script == '\n' || *script == '\r') {
                break;
            }
            *nameEnd++ = *script;
            script++;
        }
        *nameEnd = 0;
        resParseFileGroup(footName + 3, 0, &groupId);
        resEntryAdd(RES_KIND_SOUND, 0);
        resNameCursor[0] = 'e';
        resNameCursor[1] = groupIndex;
        groupIndex++;
        *(int *)groupWord = groupId;
        resNameCursor[2] = groupWord[0];
        resNameCursor[3] = groupWord[1];
        resNameCursor[4] = groupWord[2];
        resNameCursor[5] = groupWord[3];
        resNameCursor += 6;
        if (groupId == -1) {
            break;
        }
        *nameEnd++ = '_';
        for (materialIndex = 0; materialIndex < RES_FOOT_MATERIAL_MAX; materialIndex++) {
            if (footMaterialList[materialIndex] == -1) {
                break;
            }
            {
                resEntryAdd(RES_KIND_SOUND, 0);
                *resNameCursor++ = recordCode;
                recordCode++;
                for (copyPos = footName; copyPos < nameEnd; copyPos++) {
                    *resNameCursor++ = *copyPos;
                }
                materialKey = footMaterialKey(footMaterialList[materialIndex]);
                resNameCursor[0] = materialKey[0];
                resNameCursor[1] = materialKey[1];
                resNameCursor[2] = materialKey[2];
                resNameCursor[3] = 0;
                resNameCursor += 4;
            }
        }
        if (*script == '\n' || *script == '\r') {
            break;
        }
        for (;;) {
            if (*script == ',' || *script == ' ' || *script == '\t') {
                script++;
            } else {
                break;
            }
        }
    }
    return script;
}

/* Integer event name of the disk command: first byte of the command plus this base (key 0xF10000 stores the integer name). */
#define RES_DISK_EVENT_BASE 0x1D0

u8 *command_disk(u8 *entry) {
    resEntryAdd(RES_KIND_EVENT, *entry + RES_DISK_EVENT_BASE);
    return entry;
}

u8 *command_save(u8 *cursor) {
    int saveId = 0;

    if ((u8)(*cursor - '0') < 10) {
        do {
            int times5 = saveId * 5;
            saveId = times5 << 1;
            saveId = (saveId + *cursor) - '0';
            cursor++;
        } while ((u8)(*cursor - '0') < 10);
    }
    resEntryAdd(RES_KIND_EVENT, saveId + RES_EVENT_SAVE);
    return cursor;
}


/*
 * Script command ";mpeg2" (name and handler of its resCommandTable entry; proposed resCmdMpeg2).
 * It plays the movie named by the script text at the cursor and returns the cursor unchanged, as the other
 * resCmd handlers do.
 */
u8 *resCmdMpeg2(u8 *cursor) {
    GameMpeg2Play(cursor);
    return cursor;
}

/* The script text xglCdReadFile reads for resLoadCfFile (resScriptRun parses it); it starts with the "#scn"
 * header word, which is 0 while no script is loaded. */
static union {
    u32 magic;
    u8 bytes[0x800];
} resScriptBuffer;
extern ResCommand resCommandTable[];      /* script command table */
static u32 resOwnerTable[64];   /* owner list, [0] = count incl. header */
typedef struct ResCfFile {
    s16 id;
    u8 preloadState;
} ResCfFile;

/* Current CF file id and its preload state: 0 when idle or done, RES_CF_LOADING while the file is read, otherwise
   the index of the next preload entry (resPreloadStep). */
static ResCfFile resCfCurrent;

void resScriptRun(int event, int value, int tag)
{
    ResCommand *command;
    u8 *cursor;
    u8 *commandText;
    signed char *keyword;
    signed char *textChar;
    unsigned char keywordChar;
    int found;
    u32 n, next;
    int i, j, m;

    if (event != CD_EVENT_READ_DONE) {
        return;
    }
    if (resScriptBuffer.magic != RES_SCRIPT_MAGIC) {
        return;
    }
    cursor = next_line(resScriptBuffer.bytes);
    while (1) {
        if (cursor[0] == '#') {
            if (cursor[1] == 'e' && cursor[2] == 'n' && cursor[3] == 'd') {
                break;
            }
            continue;
        }
        commandText = skip_space(cursor);
        found = 0;
        for (command = resCommandTable; command->name != 0; command++) {
            keyword = command->name;
            textChar = (signed char *)commandText;
            for (;;) {
                keywordChar = *keyword;
                if (*keyword == 0) {
                    if (*textChar == ' ' || *textChar == '\t' || *textChar == '\n' || *textChar == '\r') {
                        found = 1;
                        commandText = (u8 *)textChar;
                        break;
                    }
                }
                keyword++;
                if ((u8)*textChar != keywordChar) {
                    break;
                }
                textChar++;
            }
            if (found) {
                break;
            }
        }
        if (found) {
            commandText = command->handler(skip_space(commandText));
        }
        cursor = next_line(commandText);
    }

    /* Remove the duplicates of resEntryTable, keeping the first of each value. */
    for (n = 1; n < resEntryTable[0]; n = next) {
        next = n + 1;
        for (j = next; j < resEntryTable[0]; j++) {
            if (resEntryTable[n] == resEntryTable[j]) {
                for (j++; j < resEntryTable[0]; j++) {
                    resEntryTable[j - 1] = resEntryTable[j];
                }
                resEntryTable[0]--;
                next = n;
                break;
            }
        }
    }

    /* Collect the gameResBlockTable indexes that resEntryTable does not hold. */
    n = 1;
    i = 1;
    next = gameResBlockTable[i].owner;
    if (gameResBlockTable[i].buffer != 0) {
        /* The original enters past a dead `m = 0` store; the jump into the
         * loop keeps loop.c from optimizing it (flow later drops the store). */
        goto scan;
        do {
            m = 0;
        scan:
            if (next != 0) {
                m = 1;
                if (m < resEntryTable[0]) {
                    do {
                        if (next == resEntryTable[m]) {
                            for (m++; m < resEntryTable[0]; m++) {
                                resEntryTable[m - 1] = resEntryTable[m];
                            }
                            m = 0;
                            resEntryTable[0]--;
                            break;
                        }
                    } while (++m < resEntryTable[0]);
                }
                if (i >= (u32)gameResBlockTable[0].owner && m >= resEntryTable[0]) {
                    resOwnerTable[n++] = gameResBlockTable[i].owner;
                }
            }
        } while (++i < GAME_RES_BLOCK_COUNT && (next = gameResBlockTable[i].owner, gameResBlockTable[i].buffer != 0));
    }
    resOwnerTable[0] = n;

    /* Sort resEntryTable ascending; each swap restarts the scan. */
    {
        int i, j, swapped;
        u32 tmp;

        do {
            swapped = 0;
            for (i = 1; i < resEntryTable[0]; i++) {
                for (j = i; ++j < resEntryTable[0];) {
                    if (resEntryTable[j] < resEntryTable[i]) {
                        tmp = resEntryTable[j];
                        resEntryTable[j] = resEntryTable[i];
                        resEntryTable[i] = tmp;
                        swapped = 1;
                        break;
                    }
                }
                if (swapped) {
                    break;
                }
            }
        } while (swapped);
    }
    resCfCurrent.preloadState = 1;
}


void resPreloadStep(int flag)
{
    void *block;
    int loadResult;

    if (flag == 0) {
        if (xglCdGetServicePending() != 0) {
            return;
        }
    }
    {
        u32 address = (u32)&resCfCurrent.preloadState;
        u8 *preloadIndex = (u8 *)(address + ((address & 0xFF) >> 8));
        if (GameResLoadID(resEntryTable[*preloadIndex] + RES_ID_QUEUED_LOAD, &block, 0, 0) > 0) {
            gameResBlockTable[GameResBlockFindByBuffer(block)].owner |= RES_OWNER_PRELOADED;
        }
        *preloadIndex = *preloadIndex + 1;
        if (*preloadIndex >= resEntryTable[0]) {
            *preloadIndex = 0;
        }
    }
}

extern char *cfDirectoryPath;
void resPreloadStep(int flag);
void xglCdFormatFileName(char *destination, int id);

void resLoadCfFile(int id) {
    char name[256];
    char *dst;
    char *src;

    if (resCfCurrent.id == (u16)id) {
        if (resCfCurrent.preloadState != 0 && resCfCurrent.preloadState != RES_CF_LOADING) {
            resPreloadStep(id & 0xFFFF0000);
        }
        return;
    }
    resResetScriptState();
    resCfCurrent.id = id;
    resEntryAdd(RES_KIND_SOUND, 0);
    *resNameCursor++ = 12;
    *resNameCursor++ = 0;
    resScriptBuffer.magic = 0;
    dst = name;
    src = cfDirectoryPath;
    while (*src != 0) {
        *dst++ = *src++;
    }
    xglCdFormatFileName(dst, id);
    dst[6] = '.';
    dst[7] = 't';
    dst[8] = 'x';
    dst[9] = 't';
    dst[10] = 0;
    if (xglCdGetFileSize(name) > 0) {
        resCfCurrent.preloadState = RES_CF_LOADING;
        xglCdReadFile(name, resScriptBuffer.bytes, 1, resScriptRun);
    }
}

/* Two 16-bit halves of one word; lo is the half the reader replaces. */
typedef struct {
    unsigned lo : 16;
    unsigned hi : 16;
} ResHalfWordBits;

/* Reads a 32-bit value stored as two halfwords, low at p[0] and high at p[1] (proposed resLoadHalfPair); the second argument's low half is replaced by p[0] before the result is built. */
u32 resReadHalfPairWord(u16 *p, ResHalfWordBits prev)
{
    u32 hi;

    prev.lo = p[0];
    hi = (u32)p[1] << 16;
    return prev.lo | hi;
}

typedef union { f32 f; int w; } JumpFloat;

typedef struct JumpRecord {
    u8 kind;
    /* Number of triangle centers averaged into positionRadius by resJumpPointsBuild. */
    signed char triangleCount;
    /* Scene number: the cf file the record loads (resLoadCfFile) when the player enters its radius. */
    s16 cfId;
    u8 unmodeled_04[0x0C];
    union {
        /* positionRadius[0..2] is the position, [3] the radius (then the minimum distance to another record). */
        JumpFloat positionRadius[4];
        Vector4 position;
        ResJumpPoint point;
    };
} JumpRecord;

static JumpRecord resJumpRecords[32];

typedef struct JumpTableHeader {
    u8 unmodeled_00[0x0C];
    int nameCount;
    u8 unmodeled_10[0x04];
    int cursorOffset;
    u8 unmodeled_18[0x08];
    int nameTableOffset;
} JumpTableHeader;

typedef struct JumpNameEntry {
    int nameOffset;
    int value;
} JumpNameEntry;

extern JumpTableHeader mapSceneScriptBuffer;
int strcmp(const char *, const char *);


void resParseJumpTarget(void)
{
    extern u32 resReadHalfPairWord();
    JumpTableHeader *header;
    JumpNameEntry *entry;
    u8 *names;
    u16 *start;
    u16 *cursor;
    u16 *table;
    int *stack;
    int nameIndex;
    int index;
    int matchKey;
    int recordCount;
    int value;
    u16 op;

    header = &mapSceneScriptBuffer;
    start = (u16 *) ((u8 *) header + header->cursorOffset);
    cursor = start;
    names = (u8 *) header + header->nameTableOffset;
    entry = (JumpNameEntry *) names;
    for (nameIndex = 0; nameIndex < header->nameCount; nameIndex++) {
        if (strcmp(names + entry->nameOffset, "mapjump") == 0) {
            cursor += entry->value;
            break;
        }
        entry++;
    }
    if (cursor == start) {
        return;
    }
    matchKey = sbFindNative("runtime", "jumpCF");
    do {
    } while (0);
    recordCount = 0;
    op = cursor[0];
    while (op != SB_OP_END) {
        value = resReadHalfPairWord(cursor + 1);
        if (op == SB_OP_JUMP) {
            cursor = start + value;
            for (;;) {
                op = cursor[0];
                if (op == SB_OP_END) {
                    return;
                }
                value = resReadHalfPairWord(cursor + 1);
                if (op == SB_OP_SWITCH) {
                    break;
                }
                cursor += sbOpLength(op, value);
            }
            recordCount = value;
            cursor += 5;
            break;
        }
        cursor += sbOpLength(op, value);
        op = cursor[0];
    }

    if (op == SB_OP_END) {
        return;
    }
    table = cursor;
    for (index = 0; index < recordCount; index++) {
        stack = (int *) RES_SCAN_STACK_BASE;
        resJumpRecords[index].kind = resReadHalfPairWord(table) - 1;
        resJumpRecords[index].triangleCount = 0;
        resJumpRecords[index].cfId = 0;
        resJumpRecords[index].positionRadius[0].f = resJumpRecords[index].positionRadius[1].f = resJumpRecords[index].positionRadius[2].f = -1000.0f;
        resJumpRecords[index].positionRadius[3].w = 0;
        cursor = start + resReadHalfPairWord(table + 2);
        table += 4;
        for (;;) {
            op = cursor[0];
            if (op == SB_OP_END) {
                break;
            }
            value = resReadHalfPairWord(cursor + 1);
            if (op == SB_OP_CALL_NATIVE && value == matchKey) {
                resJumpRecords[index].cfId = (u16) stack[-*stack];
                break;
            }
            if (op == SB_OP_JUMP) {
                break;
            }
            switch (op) {
            case SB_OP_CALL_NATIVE:
                stack = (int *) RES_SCAN_STACK_BASE;
                break;
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07:
            case 0x08:
            case 0x09:
            case 0x26:
                stack += 1;
                *stack = value;
                break;
            case 0x01:
            case 0x02:
            case 0x0A:
            case 0x0B:
                stack -= 1;
                break;
            }
            cursor += sbOpLength(op, value);
        }
        if (op == SB_OP_END) {
            break;
        }
    }
    resJumpRecords[index].kind = RES_JUMP_END;
}

/* Data defined after resParseJumpTarget: its "mapjump"/"runtime"/"jumpCF" literals precede these tables in .sdata. */
u8 *footGroupMaterialNames[10] = {
    "shi", "kos", "cha", "jrr", "mom",
    "zig", "jin", "r02", "r03", "r05"
};


/* Guessed type: a table of 4-byte unit keys in groups, each group ended by -1; seFootBanksLoad finds the group
 * that holds the current foot key and takes its material name. */
int footGroupKeyTable[40] = {
    0x10002, 0x10005, 0x10006, 0x10004, -1, 0x1000A, 0x1000B, -1,
    0x10010, 0x10011, 0x10012, 0x10013, -1, 0x10016, 0x10017, 0x10018,
    0x10019, -1, 0x1001C, 0x1001D, 0x1001E, -1, 0x10023, 0x10024,
    -1, 0x10026, 0x10027, 0x10028, -1, 0x14002, 0x14008, 0x14009,
    0x1400A, -1, 0x14003, -1, 0x14005, 0x1400B, -1, -1
};

u8 footMaterialBankName[16] = "fs_xxx_xxx";
u8 footBreakBankName[16] = "break_xxx";

/* Guessed type: byte table read by resFootMaterialIndex (other TU). */
unsigned char footMaterialIndexTable[56] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 10, 10, 10, 10, 10,
    0, 0, 0, 0, 0, 11, 11, 11,
    0, 0, 0, 0, 0, 0, 0, 0,
    12, 0, 0, 0, 0, 0, 0, 0,
    0, 13, 0, 0, 0, 0, 0, 0
};

GameResBlock gameResBlockTable[GAME_RES_BLOCK_COUNT] = { { 0 } };


/* Averages the triangle centers of the RES_MAP_KEY_JUMP groups per jump record: a group record (MapRecord of undulate.h) names its key
 * through the group's kind table and its triangle through the group's vertices. */
void resJumpPointsBuild(void) {
    int entryIndex;
    int key;
    Vector4 *vertices;
    MapRecordKind *keyTable;
    int i;
    int j;
    int k;
    MapFixupEntry *block;
    MapRecord *entry;
    JumpRecord *record;
    JumpFloat *vertexA;
    JumpFloat *vertexB;
    JumpFloat *vertexC;
    f32 center[3];
    f32 count;
    f32 distance;
    f32 denom;
    f32 minDistance;

    i = 0;
    do {
        block = UnduDataGetHeader(RES_MAP_KEY_JUMP, i);
        if (block != 0) {
            entry = block->records;
            vertices = block->vertices;
            keyTable = block->kindTable;
            for (entryIndex = 0; entryIndex < block->recordCount; entryIndex++) {
                key = keyTable[entry->kind].kindCode;
                if ((key & ~0x1F) == RES_MAP_KEY_JUMP) {
                    for (k = 0; resJumpRecords[k].kind != RES_JUMP_END; k++) {
                        record = &resJumpRecords[k];
                        if (record->kind == (key & 0xFF)) {
                            vertexA = (JumpFloat *) &vertices[entry->vertexIndex[0]];
                            vertexB = (JumpFloat *) &vertices[entry->vertexIndex[1]];
                            vertexC = (JumpFloat *) &vertices[entry->vertexIndex[2]];
                            center[0] = (vertexA[0].f + vertexB[0].f + vertexC[0].f) / 3.0f;
                            center[1] = (vertexA[1].f + vertexB[1].f + vertexC[1].f) / 3.0f;
                            center[2] = (vertexA[2].f + vertexB[2].f + vertexC[2].f) / 3.0f;
                            count = I2F((u8) record->triangleCount);
                            denom = count + 1.0f;
                            center[0] = center[0] + record->positionRadius[0].f * count;
                            center[1] = center[1] + record->positionRadius[1].f * count;
                            center[2] = center[2] + record->positionRadius[2].f * count;
                            record->triangleCount = (signed char) ((u8) record->triangleCount + 1);
                            record->positionRadius[0].f = center[0] / denom;
                            record->positionRadius[1].f = center[1] / denom;
                            record->positionRadius[2].f = center[2] / denom;
                            break;
                        }
                    }
                }
                entry++;
            }
        }
        i++;
    } while (i < 0x10);

    for (i = 0; resJumpRecords[i].kind != RES_JUMP_END; i++) {
        minDistance = 100000.0f;
        for (j = 0; resJumpRecords[j].kind != RES_JUMP_END; j++) {
            if (i != j && resJumpRecords[i].cfId != resJumpRecords[j].cfId) {
                distance = xglPointLength(&resJumpRecords[i].position, &resJumpRecords[j].position) * 0.5f;
                if (distance < minDistance) {
                    minDistance = distance;
                }
            }
        }
        resJumpRecords[i].positionRadius[3].f = minDistance;
    }
}

void resJumpRecordSet(int index, int cfId, ResJumpPoint *point) {
    if (point == 0) {
        return;
    }
    if (index < 0) {
        for (index = 0; resJumpRecords[index].kind != RES_JUMP_END; index++) {
        }
        resJumpRecords[index + 1].kind = RES_JUMP_END;
    }
    resJumpRecords[index].kind = RES_JUMP_NAMED;
    resJumpRecords[index].triangleCount = 0;
    resJumpRecords[index].cfId = cfId;
    resJumpRecords[index].point = *point;
}


/* Loads the cf file of the first jump point whose radius reaches the current map object. A jump
   record holds its point in positionRadius[0..2], the radius in positionRadius[3] and the cf file in cfId;
   gameWork.mapId is the map id. */
void resJumpPointsUpdate(void)
{
    AlignedVector4 *position;
    int i;

    seVoiceRecordsUpdatePan();
    if (resJumpRecords[0].kind == RES_JUMP_END) {
        resParseJumpTarget();
        resJumpPointsBuild();
        resJumpRecordSet(0, 0, 0);
    }
    if (gameWork.mapId == 1520 || gameWork.mapId == 1560) {
        return;
    }
    if (xglCdSyncDone() == 0) {
        return;
    }
    position = &uwamonoRecords[uwamonoCurrentIndex].body.actor->position;
    for (i = 0; resJumpRecords[i].kind != RES_JUMP_END; i++) {
        if (xglPointLength(position, &resJumpRecords[i].position) < resJumpRecords[i].positionRadius[3].f) {
            if (resJumpRecords[i].cfId > 0) {
                resLoadCfFile(resJumpRecords[i].cfId);
                return;
            }
        }
    }
}


typedef struct {
    u8 bytes[0x10];
} ResBlock;

typedef struct {
    u16 value;
    u8 mode;
} ResNameLatch;


void resResetNameTables(void)
{
    GameResBlock *head;
    GameResBlock *walk;
    GameResBlock *entry;
    GameResBlock *end;
    int found;
    int index;
    u32 flags;
    u32 keepFlag;

    resEntryTable[0] = 1;
    resOwnerTable[0] = 1;
    resNameCursor = resNamePool;
    head = gameResBlockTable;
    do {
        found = 0;
        index = head[0].owner;
        if (index < 128 && head[index].buffer != 0) {
            walk = gameResBlockTable;
            entry = &walk[index];
            keepFlag = 0x40000000;
            end = &walk[128];
            do {
                flags = entry->owner;
                entry++;
                if ((flags & keepFlag) != 0) {
                    GameResBlockFreeByOwner(flags);
                    found = 1;
                    break;
                }
            } while ((int)entry < (int)end && entry->buffer != 0);
        }
    } while (found != 0);
    resCfCurrent.id = -1;
    resCfCurrent.preloadState = 0;
}

void resResetNameTables(void);

void resResetScriptState(void) {
    xglCdReadCancel(0);
    resResetNameTables();
}

/* Adds the record's address to its pointer words (toAbsolute != 0) or subtracts it, for kinds 0x10000, 0x20000 and 0x70000 (proposed resRelocateRecord). */
void resRelocateRecord(int toAbsolute, int kindWord, int *record) {
    int delta;
    u32 kind;

    kind = kindWord & CD_FILE_TYPE_MASK;
    switch (kind) {
    case RES_KIND_ACT:
    case RES_KIND_MAP:
    case RES_KIND_MOBJ:
        break;
    default:
        return;
    }
    delta = (int) record;
    if (toAbsolute == 0) {
        delta = -delta;
    }
    if (record[0] != 0) {
        record[0] += delta;
    }
    if (record[1] != 0) {
        record[1] += delta;
    }
    if (record[2] != 0) {
        record[2] += delta;
    }
    if (record[3] != 0) {
        record[3] += delta;
    }
    if (record[4] != 0) {
        record[4] += delta;
    }
}

static unsigned char resEnemyFootCount;

void resApplyCfFile(int id)
{
    int i;
    int k;
    int cur;
    int changed;
    int flags;
    int size;
    int found;
    int idx;
    u32 *base;
    u32 *owners;
    u32 *owner;
    u32 *entries;
    unsigned int entry;
    unsigned int kind;
    char *name;
    int code;
    int *oldRecord;
    GameResBlock *slot;

    while (resCfCurrent.id != id || resCfCurrent.preloadState != 0) {
        resLoadCfFile(0x10000 | id);
        xglCdServiceIfPending();
    }
    resResetDescriptorTables();
    mainObject = 0;
    gameWork.active = 0;
    mapLoadBlock(0);
    resJumpRecords[0].kind = RES_JUMP_END;
    seBgmVolume = -1;
    while (xglCdServiceIfPending() != 0)
        ;
    if (resScriptBuffer.magic == 0) {
        GameResourceReset();
        resClearRecordTable();
    }

    base = resOwnerTable;
    i = 1;
    if (i < base[0]) {
        owners = base;
        base = base + 1;
        owner = base;
        do {
            GameResBlockFreeByOwner(*owner);
            owner++;
            i++;
        } while (i < owners[0]);
    }

    do {
        changed = 0;
        slot = gameResBlockTable;
        i = slot[0].owner;
        for (; i < GAME_RES_BLOCK_COUNT && gameResBlockTable[i].buffer != 0; i++) {
            flags = gameResBlockTable[i].owner;
            if (flags & RES_OWNER_PRELOADED) {
                size = gameResBlockTable[i].size;
                found = 0;
                idx = gameResBlockTable[0].owner;
                if (idx < i && gameResBlockTable[idx].buffer != 0) {
                    while (idx < i && gameResBlockTable[idx].buffer != 0) {
                        if (gameResBlockTable[idx].owner == 0
                            && ((int) gameResBlockTable[idx].size >= size || idx + 1 == i)) {
                            found = idx;
                            break;
                        }
                        idx++;
                    }
                }
                if (found == 0) {
                    gameResBlockTable[i].owner &= 0xBFFFFFFF;
                } else {
                    oldRecord = gameResBlockTable[i].buffer;
                    GameResBlockFreeByOwner(flags);
                    GameResBlockAssign(found, flags & 0xBFFFFFFF, size);
                    resRelocateRecord(0, flags, oldRecord);
                    xglMemCopy16(gameResBlockTable[found].buffer, oldRecord, size / 16);
                    resRelocateRecord(1, flags, gameResBlockTable[found].buffer);
                    changed = 1;
                    break;
                }
            }
        }
    } while (changed);

    entries = resEntryTable;
    for (i = 1; i < resEntryTable[0]; i++) {
        entry = entries[i];
        name = resNamePool + (entry & 0xFFFF);
        switch (entry & CD_FILE_TYPE_MASK) {
        case RES_KIND_SOUND:
            switch (name[0]) {
            case 0 ... 63:
                if (name[1] == 0) {
                    if (name[0] == SE_BANK_FOOT_MATERIAL) {
                        for (k = SE_BANK_FOOT_MATERIAL; k < SE_BANK_FOOT_MATERIAL + 5; k++) {
                            seBankSetName(k, 0);
                        }
                    } else if (name[0] == SE_BANK_ENEMY_FOOT) {
                        for (k = SE_BANK_ENEMY_FOOT; k < SE_BANK_ENEMY_FOOT + 5; k++) {
                            seBankSetName(k, 0);
                        }
                    } else {
                        seBankSetName(name[0], 0);
                    }
                } else {
                    seBankSetName(name[0], name + 1);
                }
                break;
            /* footstep material k = name[1] has material name[2]; the list ends in 0xFF */
            case 'f':
                k = name[1];
                resFootMaterials[k] = name[2];
                resFootMaterials[k + 1] = 0xFF;
                resEnemyFootCount = k + 1;
                break;
            /* enemy footstep group name[1] = file group id in name[2..5] */
            case 'e': {
                unsigned char word[4];

                word[0] = name[2];
                word[1] = name[3];
                word[2] = name[4];
                word[3] = name[5];
                resEnemyFootGroups[name[1]] = *(int *) word;
                break;
            }
            /* BGM name list: skip names already played, then start the next (seBgmNameSet) */
            case 's':
                name++;
                while (bgmNameListIndex >= 2) {
                    while (*name++ != 0) {
                    }
                    bgmNameListIndex--;
                }
                seBgmNameSet(name);
                break;
            /* BGM volume */
            case 'v':
                seBgmVolume = name[1];
                break;
            }
            break;
        case RES_KIND_EFFECT:
            sefLoadEffectCfName(0, name);
            break;
        case GAME_RES_OWNER:
            GameResLoadName(name, 0, 0, 0);
            break;
        case RES_KIND_EVENT:
            code = ((unsigned char *) &entries[i])[0];
            switch (entry & 0xFF00) {
            case RES_EVENT_SAVE:
                Intermission(code);
                break;
            case RES_EVENT_DISK:
                GameDiskChange(code);
                break;
            }
            break;
        default:
            GameResLoadID(entries[i], 0, 0, 0);
            break;
        }
    }
    resResetScriptState();
}

void resGetEntryTables(u32 **first, u32 **second) {
    if (first != 0) {
        *first = resEntryTable;
    }
    if (second != 0) {
        *second = resOwnerTable;
    }
}

/* Ten-byte pad text copied after each path buffer. */
typedef struct {
    char text[10];
} ResPadText;

int sprintf(char *destination, const char *format, ...);
const char playerSlotPathFormat[16] = "(Player#%d)";
const ResPadText pathPadText = { "(padding)" };

/* Loads resource 0x30041 and sets up the three path buffers (each followed by its pad text) and,
   in slot 3, the largest free block. */
void resPlayerSlotsInit(void)
{
    char *buffer;
    int i;

    GameResLoadID(0x30041, 0, 0, 0);
    for (i = 0; i < 3; i++) {
        buffer = GameResBufferAlloc(PLAYER_MODEL_BUFFER_SIZE);
        resPlayerModelSlots[i].resource_id = 0;
        resPlayerModelSlots[i].base = buffer;
        sprintf(buffer, playerSlotPathFormat, i);
        buffer = GameResBufferAlloc(RES_PATH_PAD_SIZE);
        *(ResPadText *) buffer = pathPadText;
    }
    resPlayerModelSlots[3].base = GameResourceGetFreeAddr(0);
}

int resPlayerSlotFindByOwner(int owner) {
    int i;

    for (i = 0; i < 3; i++) {
        int index = GameResBlockFindByBuffer(resPlayerModelSlots[i].base);
        if (index > 0 && gameResBlockTable[index].owner == owner) {
            return i;
        }
    }
    return -1;
}


void uwamonoActorsResetByKey(int owner) {
    int i;

    for (i = 0; i < 256; i++) {
        if (uwamonoRecords[i].key == owner) {
            struct XenoActor *actor = uwamonoRecords[i].body.actor;

            if (actor != 0) {
                actor->kind = 0;
                actor->rsrc[XAct_Rsrc_EntryTableC] = 0;
                actor->rsrc[XAct_Rsrc_XAP] = 0;
                actor->rsrc[XAct_Rsrc_XHR] = 0;
                actor->rsrc[XAct_Rsrc_TEX] = 0;
                actor->rsrc[XAct_Rsrc_MDL] = 0;
                XenoActorAllocMatrices(actor);
                tiMRPackageInit(XenoActorMotionPack(actor));
                tiMRPackageSetAnimData(XenoActorMotionPack(actor), 0);
            }
        }
    }
}

#include "debug_break.h"
#include "main/xgl_flags.h"

void *GameResBlockAssign(int index, int owner, unsigned int size);
void GameResBlockFree(int index);
void uwamonoActorsResetByKey(int handle);

typedef struct {
    char text[9];
} PlayerName;

extern const char playerSlotDefaultName[];

void resPlayerSlotSet(int slot, u8 *base, u8 *limit, void *value)
{
    int i;

    do {
        for (i = 1; i < GAME_RES_BLOCK_COUNT; i++) {
            u8 *blockStart;
            int owner;

            if (gameResBlockTable[i].buffer == 0) {
                i = GAME_RES_BLOCK_COUNT;
                break;
            }
            blockStart = gameResBlockTable[i].buffer;
            if (blockStart < base || blockStart >= limit) {
                continue;
            }
            owner = gameResBlockTable[i].owner;
            if (owner == 0) {
                continue;
            }
            if (base != value) {
                uwamonoActorsResetByKey(owner);
            }
            GameResBlockFree(i);
            break;
        }
    } while (i < GAME_RES_BLOCK_COUNT);
    if (base != value) {
        i = GameResBlockFindByBuffer(base);
        if (i == 0) {
            printf("block lost\n");
            DEBUG_BREAK();
        }
        GameResBlockAssign(i, GAME_RES_OWNER, PLAYER_MODEL_BUFFER_SIZE);
        *(PlayerName *)base = *(PlayerName *)playerSlotDefaultName;
    }
    for (i = 1; i < GAME_RES_BLOCK_COUNT; i++) {
        if (gameResBlockTable[i].buffer == value) {
            gameResBlockTable[0].owner = i;
            break;
        }
    }
    resPlayerModelSlots[slot].resource_id = 0;
}

const char playerSlotDefaultName[16] = "(Player)";


void resPlayerSlotUpdate(int slot)
{
    u8 *base;

    if (slot < 0) {
        base = resPlayerModelSlots[2].base;
        resPlayerSlotSet(2, base, base + 0xC6000, base);
        if (slot < -1) {
            base = resPlayerModelSlots[1].base;
            resPlayerSlotSet(1, base, base + 0xC6000, base);
        }
        if (slot < -2) {
            base = resPlayerModelSlots[0].base;
            resPlayerSlotSet(0, base, base + 0xC6000, base);
        }
        return;
    }
    base = resPlayerModelSlots[slot].base;
    resPlayerSlotSet(slot, base, base + 0xC5FC0, gameResBlockTable[gameResBlockTable[0].owner].buffer);
}

static inline void resRecordSlotBlock(int slot, int mask, void **out, u8 *block) {
    if (out != 0) {
        *out = block;
    }
    if (block == resPlayerModelSlots[slot].base) {
        resPlayerModelSlots[slot].resource_id = mask;
    }
}

void resPlayerSlotLoadID(int slot, int value, void **out) {
    int resourceId;
    void *savedSelected;
    u8 *slotBase;
    u8 *limit;
    int index;
    u8 *blockStart;
    int owner;
    int size;
    void *loaded;

    resourceId = value & GAME_RES_ID_MASK;
    savedSelected = gameResBlockTable[gameResBlockTable[0].owner].buffer;
    slotBase = GameResFindID(resourceId, 0);
    if (slotBase != 0) {
        if (out != 0) {
            *out = slotBase;
        }
        if (slotBase == resPlayerModelSlots[slot].base) {
            resPlayerModelSlots[slot].resource_id = resourceId;
        }
        return;
    }
    slotBase = resPlayerModelSlots[slot].base;
    limit = slotBase + PLAYER_MODEL_BUFFER_SIZE;
    for (index = 1; index < GAME_RES_BLOCK_COUNT && gameResBlockTable[index].buffer != 0; index++) {
        blockStart = gameResBlockTable[index].buffer;
        if (blockStart >= slotBase && blockStart < limit) {
            owner = gameResBlockTable[index].owner;
            if ((owner & GAME_RES_OWNER_MASK) == GAME_RES_OWNER || owner == 0) {
                size = xglCdLoadByKind(value, 0, 0, 0);
                if (size != 0) {
                    loaded = GameResBlockAssign(index, resourceId, size);
                    xglCdLoadByKind(value, (unsigned int) loaded, 0, 0);
                    if (out != 0) {
                        *out = loaded;
                    }
                    if (loaded == resPlayerModelSlots[slot].base) {
                        resPlayerModelSlots[slot].resource_id = resourceId;
                    }
                }
                break;
            }
        }
    }
    for (index = 1; index < GAME_RES_BLOCK_COUNT; index++) {
        if (gameResBlockTable[index].buffer == savedSelected) {
            gameResBlockTable[0].owner = index;
            break;
        }
    }
}

/* A player model slot: the buffer block, directly followed by the pad block holding the pad text. */
typedef struct {
    unsigned char data[PLAYER_MODEL_BUFFER_SIZE];
    ResPadText pad;
} PlayerModelSlotBuffer;

void resPlayerSlotBlockReset(void *buffer) {
    int block;

    block = GameResBlockFindByBuffer(buffer);
    if (block != 0 && gameResBlockTable[block].owner == 0) {
        GameResBlockAssign(block, GAME_RES_OWNER, PLAYER_MODEL_BUFFER_SIZE);
        *(PlayerName *) buffer = *(PlayerName *) playerSlotDefaultName;
        GameResBlockAssign(block + 1, GAME_RES_OWNER, RES_PATH_PAD_SIZE);
        ((PlayerModelSlotBuffer *) buffer)->pad = pathPadText;
    }
}

void resPlayerSlotBlockReset(void *buffer);

void resPlayerSlotsReset(void) {
    int i;

    resPlayerSlotBlockReset(resPlayerModelSlots[0].base);
    resPlayerSlotBlockReset(resPlayerModelSlots[1].base);
    resPlayerSlotBlockReset(resPlayerModelSlots[2].base);
    for (i = 1; i < 0x80; i++) {
        if (gameResBlockTable[i].buffer == resPlayerModelSlots[3].base) {
            gameResBlockTable[0].owner = i;
            break;
        }
    }
    resPlayerSlotsLoadModels(resPlayerModelSlots[0].resource_id, resPlayerModelSlots[1].resource_id, resPlayerModelSlots[2].resource_id);
}

int seBankSetName(int kind, char *text);
float seSetMaxDistance(float maxDistance);

void seFootBanksLoad(void) {
    int footKey;
    int materialGroup;
    int listIndex;
    int i;
    const u8 *groupMaterial;

    footKey = uwamonoRecords[uwamonoCurrentIndex].key;
    groupMaterial = 0;
    materialGroup = 0;
    listIndex = 0;
    if (-1 != footGroupKeyTable[0]) {
        if (footGroupKeyTable[0] == footKey) {
            groupMaterial = footGroupMaterialNames[0];
        } else {
            int listKey;

            /* A goto-formed loop: no loop notes, so loop.c leaves it alone as in the original; for(;;) and
             * do-while versions hoist the table addresses and change the object. */
        next:
            if (footGroupKeyTable[listIndex + 1] == -1) {
                listIndex++;
                materialGroup++;
            }
            listIndex++;
            listKey = footGroupKeyTable[listIndex];
            if (listKey == -1) {
                goto done;
            }
            if (listKey != footKey) {
                goto next;
            }
            groupMaterial = footGroupMaterialNames[materialGroup];
        }
    }
done:
    footMaterialBankName[3] = groupMaterial[0];
    footBreakBankName[6] = footMaterialBankName[3];
    footMaterialBankName[4] = footBreakBankName[7] = groupMaterial[1];
    footMaterialBankName[5] = footBreakBankName[8] = groupMaterial[2];
    if (materialGroup == 6) {
        if (xglFlagsGet8(401) > 13) {
            footMaterialBankName[5] = 0x32;
        }
    }
    if (materialGroup < 7) {
        seSetMaxDistance(10);
        seTonePlay(-1);
    } else {
        seSetMaxDistance(60);
        seTonePlay(-2);
    }
    seBankSetName(SE_BANK_FOOT, 0);
    for (i = SE_BANK_FOOT_MATERIAL; i < SE_BANK_ENEMY_FOOT; i++) {
        seBankSetName(i, 0);
    }
    seBankSetName(SE_BANK_FOOT, footBreakBankName);
    for (i = SE_BANK_FOOT_MATERIAL; i < SE_BANK_ENEMY_FOOT; i++) {
        seBankSetName(i, 0);
    }
    for (i = 0; i < RES_FOOT_MATERIAL_MAX && resFootMaterials[i] != -1; i++) {
        {
            u8 *footMaterial = footMaterialKey(resFootMaterials[i]);

            footMaterialBankName[7] = footMaterial[0];
            footMaterialBankName[8] = footMaterial[1];
            footMaterialBankName[9] = footMaterial[2];
        }
        seBankSetName(i + SE_BANK_FOOT_MATERIAL, footMaterialBankName);
    }
}

extern unsigned char footMaterialIndexTable[];

int resFootMaterialIndex(int code) {
    int i;

    if ((code & 0xFFFFFF00) == 0x400) {
        code = footMaterialIndexTable[code & 0xFF];
        for (i = 0; i < RES_FOOT_MATERIAL_MAX && resFootMaterials[i] != -1; i++) {
            if (resFootMaterials[i] == code) {
                return i;
            }
        }
    }
    return -1;
}

extern unsigned char resEnemyFootCount;
int resFootMaterialIndex(int key);

int seFootBankFromKind(int kind, int code) {
    int i;
    int kindClass;
    int slot;

    kindClass = kind & 0xFFFFF000;
    if (kindClass == 0x10000 || kindClass == 0x14000) {
        if (!(uwamonoRecords[uwamonoCurrentIndex].flags & 2)) {
            return -1;
        }
        slot = resFootMaterialIndex(code);
        xglFontDebugPrintf(64, 0, "%2x %2d", code, slot);
        return slot < 0 ? slot : slot + SE_BANK_FOOT_MATERIAL;
    }
    if (kind == 0x18037) {
        kind = 0x12047;
    }
    if (kind == 0x18038) {
        kind = 0x12048;
    }
    if (kind == 0x1901E) {
        kind = 0x12049;
    }
    if (kind == 0x18020) {
        kind = 0x1204A;
    }
    if (kind == 0x18021) {
        kind = 0x1204B;
    }
    i = 0;
    while (resEnemyFootGroups[i] != -1) {
        if (resEnemyFootGroups[i] == kind) {
            slot = resFootMaterialIndex(code);
            if (slot < 0) {
                return slot;
            }
            return resEnemyFootCount * i + slot + SE_BANK_ENEMY_FOOT;
        }
        i++;
    }
    return -1;
}

/* Follows the "%2x %2d" literal of seFootBankFromKind in .sdata. */
unsigned char bgmNameListIndex = 0;


