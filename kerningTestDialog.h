#pragma once

#include <iostream>

#include <QDialog>

#include "IBMFDriver/ibmf_font_mod.hpp"
#include "drawingSpace.h"

namespace Ui {
class KerningTestDialog;
}

class KerningTestDialog : public QDialog {
  Q_OBJECT

public:
  explicit KerningTestDialog(IBMFFontModPtr font, int faceIdx, QWidget *parent = nullptr);
  ~KerningTestDialog();

  void setText(QString text);

private slots:
  void on_pixelSizeCombo_currentIndexChanged(int index);
  void on_autoKernCheckBox_toggled(bool checked);
  void on_normalKernCheckBox_toggled(bool checked);

  void on_comboBox_currentIndexChanged(int index);

private:
  Ui::KerningTestDialog *ui;

  DrawingSpace *drawingSpace_;

  QString combinedLetters();
};

const constexpr char *proofingTexts[] = {
    u8"Lowercase letters:\n\n"
    "Angel Adept Blind Bodice Clique Coast Dunce Docile Enact Eosin Furlong Focal Gnome "
    "Gondola Human Hoist Inlet Iodine Justin Jocose "
    "Knoll Koala Linden Loads Milliner Modal Number Nodule Onset Oddball Pneumo Poncho "
    "Quanta Qophs Rhone Roman Snout Sodium Tundra Tocsin Uncle Udder Vulcan Vocal Whale Woman Xmas "
    "Xenon Yunnan Young Zloty Zodiac. Angel angel adept for the nuance loads of the arena cocoa "
    "and quaalude. Blind blind bodice for the submit oboe of the club snob and abbot. Clique "
    "clique coast for the pouch loco of the franc assoc and accede. Dunce dunce docile for the "
    "loudness mastodon of the loud statehood and huddle. Enact enact eosin for the quench coed of "
    "the pique canoe and bleep. Furlong furlong focal for the genuflect profound of the motif "
    "aloof and offers. Gnome gnome gondola for the impugn logos of the unplug analog and smuggle. "
    "Human human hoist for the buddhist alcohol of the riyadh caliph and bathhouse. Inlet inlet "
    "iodine for the quince champion of the ennui scampi and shiite. Justin justin jocose for the "
    "djibouti sojourn of the oranj raj and hajjis. Knoll knoll koala for the banknote lookout of "
    "the dybbuk outlook and trekked. Linden linden loads for the ulna monolog of the consul "
    "menthol and shallot. Milliner milliner modal for the alumna solomon of the album custom and "
    "summon. Number number nodule for the unmade economic of the shotgun bison and tunnel. Onset "
    "onset oddball for the abandon podium of the antiquo tempo and moonlit. Pneumo pneumo poncho "
    "for the dauphin opossum of the holdup bishop and supplies. Quanta quanta qophs for the "
    "inquest sheqel of the cinq coq and suqqu. Rhone rhone roman for the burnt porous of the lemur "
    "clamor and carrot. Snout snout sodium for the ensnare bosom of the genus pathos and missing. "
    "Tundra tundra tocsin for the nutmeg isotope of the peasant ingot and ottoman. Uncle uncle "
    "udder for the dunes cloud of the hindu thou and continuum. Vulcan vulcan vocal for the "
    "alluvial ovoid of the yugoslav chekhov and revved. Whale whale woman for the meanwhile "
    "blowout of the forepaw meadow and glowworm. Xmas xmas xenon for the bauxite doxology of the "
    "tableaux equinox and exxon. Yunnan yunnan young for the dynamo coyote of the obloquy employ "
    "and sayyid. Zloty zloty zodiac for the gizmo ozone of the franz laissez and buzzing.",

    u8"Uppercase letters:\n\n"
    "ABIDE ACORN OF THE HABIT DACRON FOR THE BUDDHA GOUDA QUAALUDE. BENCH BOGUS OF THE SCRIBE "
    "ROBOT FOR THE APLOMB JACOB RIBBON. CENSUS CORAL OF THE SPICED JOCOSE FOR THE BASIC HAVOC "
    "SOCCER. DEMURE DOCILE OF THE TIDBIT LODGER FOR THE CUSPID PERIOD BIDDER. EBBING ECHOING OF "
    "THE BUSHED DECAL FOR THE APACHE ANODE NEEDS. FEEDER FOCUS OF THE LIFER BEDFORD FOR THE SERIF "
    "PROOF BUFFER. GENDER GOSPEL OF THE PIGEON DOGCART FOR THE SPRIG QUAHOG DIGGER. HERALD HONORS "
    "OF THE DIHEDRAL MADHOUSE FOR THE PENH RIYADH BATHHOUSE. IBSEN ICEMAN OF THE APHID NORDIC FOR "
    "THE SUSHI SAUDI SHIITE. JENNIES JOGGER OF THE TIJERA ADJOURN FOR THE ORANJ KOWBOJ HAJJIS. "
    "KEEPER KOSHER OF THE SHRIKE BOOKCASE FOR THE SHEIK LOGBOOK CHUKKAS. LENDER LOCKER OF THE "
    "CHILD GIGOLO FOR THE UNCOIL GAMBOL ENROLLED. MENACE MCCOY OF THE NIMBLE TOMCAT FOR THE DENIM "
    "RANDOM SUMMON. NEBULA NOSHED OF THE INBRED BRONCO FOR THE COUSIN CARBON KENNEL. OBSESS OCEAN "
    "OF THE PHOBIC DOCKSIDE FOR THE GAUCHO LIBIDO HOODED. PENNIES PODIUM OF THE SNIPER OPCODE FOR "
    "THE SCRIP BISHOP HOPPER. QUANTA QOPHS OF THE INQUEST OQOS FOR THE CINQ COQ SUQQU. REDUCE "
    "ROGUE OF THE GIRDLE ORCHID FOR THE MEMOIR SENSOR SORREL. SENIOR SCONCE OF THE DISBAR GODSON "
    "FOR THE HUBRIS AMENDS LESSEN. TENDON TORQUE OF THE UNITED SCOTCH FOR THE NOUGHT FORGOT "
    "BITTERS. UNDER UGLINESS OF THE RHUBARB SEDUCE FOR THE MANCHU HINDU CONTINUUM. VERSED VOUCH "
    "OF THE DIVER OVOID FOR THE TELAVIV KARPOV FLIVVER. WENCH WORKER OF THE UNWED SNOWCAP FOR THE "
    "ANDREW ESCROW GLOWWORM. XENON XOCHITL OF THE MIXED BOXCAR FOR THE SUFFIX ICEBOX EXXON. "
    "YEOMAN YONDER OF THE HYBRID ARROYO FOR THE DINGHY BRANDY SAYYID. ZEBRA ZOMBIE OF THE PRIZED "
    "OZONE FOR THE FRANZ ARROZ BUZZING.",

    u8"ASCII English Pangrams (Holoalphabetics):\n\n"
    "Two driven jocks help fax my big quiz.\n"
    "Fickle jinx bog dwarves spy math quiz.\n"
    "Public junk dwarves hug my quartz fox.\n"
    "Quick fox jumps nightly above wizard.\n"
    "Five quacking zephyrs jolt my wax bed.\n"
    "The five boxing wizards jump quickly.\n"
    "Pack my box with five dozen liquor jugs.\n"
    "The quick brown fox jumps over the lazy dog.\n"
    "When zombies arrive, quickly fax judge Pat.\n"
    "Woven silk pyjamas exchanged for blue quartz.\n"
    "The quick onyx goblin jumps over the lazy dwarf.\n"
    "Foxy diva Jennifer Lopez wasn’t baking my quiche.\n"
    "My girl wove six dozen plaid jackets before she quit.\n"
    "Grumpy wizards make a toxic brew for the jovial queen.\n"
    "A quivering Texas zombie fought republic linked jewelry.\n"
    "All questions asked by five watched experts amaze the judge.\n"
    "Back in June we delivered oxygen equipment of the same size.\n"
    "The wizard quickly jinxed the gnomes before they vaporized.\n"
    "We promptly judged antique ivory buckles for the next prize.\n"
    "Jim quickly realized that the beautiful gowns are expensive.\n",

    u8"European Pangrams (Holoalphabetics):\n"
    "\nAzari:\n"
    "Zəfər, jaketini də papağını da götür, bu axşam hava çox soyuq olacaq. (\"Zəfər, take your "
    "jacket and cap, it will be very cold tonight.\")\n"
    "\nCzech:\n"
    "Příliš žluťoučký kůň úpěl ďábelské ódy. (\"A horse that was too yellow moaned devilish "
    "odes.\")\n"
    "Nechť již hříšné saxofony ďáblů rozezvučí síň úděsnými tóny waltzu, tanga a quickstepu. "
    "(\"May the sinful saxophones of devils echo through the hall with dreadful melodies of waltz, "
    "tango and quickstep.\")\n"
    "Hleď, toť přízračný kůň v mátožné póze šíleně úpí. (\"Behold, tis the eerie horse in "
    "tottering affectation groaning like crazy.\")\n"
    "Zvlášť zákeřný učeň s ďolíčky běží podél zóny úlů. (\"Particularly insidious apprentice with "
    "dimples is running along the zone of hives.\")\n"
    "Loď čeří kýlem tůň obzvlášť v Grónské úžině. (\"A vessel ripples a pool by its keel, "
    "especially in the strait of Greenland.\")\n"
    "Ó, náhlý déšť již zvířil prach a čilá laň teď běží s houfcem gazel k úkrytům. (\"Oh, sudden "
    "rain has already whirled the dust and a spry doe now gallops with a flock of gazelles for the "
    "shelter.\")\n"
    "\nDanish:\n"
    "Høj bly gom vandt fræk sexquiz på wc. (\"Tall shy groom won naughty sexquiz on wc.\")\n"
    "\nEsperanto:\n"
    "Eble ĉiu kvazaŭ-deca fuŝĥoraĵo ĝojigos homtipon. (\"Maybe every quasi-fitting bungle-choir "
    "makes a human type happy.\")\n"
    "\nEwe:\n"
    "Dzigbe zã nyuie na wò, ɣeyiɣi didi aɖee nye sia no see, ɣeyiɣi aɖee nye sia tso esime míeyi "
    "suku. Ŋdɔ nyui, ɛ nyteƒe, míagakpɔ wò ake wuieve kele ʋ heda kpedeŋu. (\"Have a nice birthday "
    "tonight, it's been a long time no see, it's been a while since we were in school. Good "
    "afternoon, yes, see you again at twelve o'clock in the morning.\")\n"
    "\nFinnish:\n"
    "Törkylempijävongahdus. (Although difficult to translate because of its non-practical use, "
    "but roughly means to \"a whinge of a sleazy lover\")\n"
    "On sangen hauskaa, että polkupyörä on maanteiden jokapäiväinen ilmiö. (\"It is rather fun "
    "that bicycles are a daily phenomenon on the countryroads.\")\n"
    "Wieniläinen siouxia puhuva ökyzombi diggaa Åsan roquefort-tacoja. (\"Viennese rich zombie who "
    "can speak Sioux likes Åsa's Roquefort tacos.\")\n"
    "\nGerman:\n"
    "Victor jagt zwölf Boxkämpfer quer über den großen Sylter Deich. (\"Victor chases twelve "
    "boxers "
    "across the Great Levee of Sylt.\")\n"
    "\nIcelandic:\n"
    "Kæmi ný öxi hér, ykist þjófum nú bæði víl og ádrepa. (\"If a new axe were here, thieves would "
    "feel increasing deterrence and punishment.\")\n"
    "\nIrish:\n"
    "d'Ith cat mór dubh na héisc lofa go pras. (\"A large black cat ate the rotten fish "
    "promptly.\")\n"
    "\nItalian:\n"
    "Pranzo d'acqua fa volti sghembi. (\"A lunch of water makes twisted faces.\")\n"
    "\nKurdish:\n"
    "Cem vî Feqoyê pîs zêdetir ji çar gulên xweşik hebûn. (\"There were more than four beautiful "
    "flowers near the filthy Feqo.\")\n"
    "\nPolish:\n"
    "Stróż pchnął kość w quiz gędźb vel fax myjń. (\"The watchman pushed the bone into a quiz of "
    "the musics or a fax of the washes.\")\n"
    "\nSpanish:\n"
    "Benjamín pidió una bebida de kiwi y fresa. Noé, sin vergüenza, la más exquisita champaña del "
    "menú. (\"Benjamin ordered a kiwi and strawberry drink. Noah, without shame, the most "
    "exquisite champagne on the menu.\")\n"
    "José compró una vieja zampoña en Perú. Excusándose, Sofía tiró su whisky al desagüe de la "
    "banqueta. (\"José bought an old panpipe in Peru. Excusing herself, Sofía threw her whiskey on "
    "the sink of the sidewalk.\")\n"
    "El veloz murciélago hindú comía feliz cardillo y kiwi. La cigüeña tocaba el saxofón detrás "
    "del palenque de paja. (\"The quick Hindu bat was happily eating golden thistle and kiwi. The "
    "stork was playing the saxophone behind the straw arena.\")\n"
    "El pingüino Wenceslao hizo kilómetros bajo exhaustiva lluvia y frío; añoraba a su querido "
    "cachorro. (\"Wenceslao the penguin traveled kilometers under exhaustive rain and cold; he "
    "longed for his dear puppy.\")\n"
    "La niña, viéndose atrapada en el áspero baúl índigo y sintiendo asfixia, lloró de vergüenza; "
    "mientras que la frustrada madre llamaba a su hija diciendo: “¿Dónde estás Waleska?”. (\"The "
    "girl, finding herself trapped inside the rough blue-violet chest and feeling suffocation, "
    "cried out of shame; whilst the frustrated mother called her daughter saying: “Where are you "
    "Waleska?”.\")\n"
    "Jovencillo emponzoñado de whisky: ¡qué figurota exhibe! (\"Whisky-intoxicated youngster – "
    "what a figure he’s showing!\")\n"
    "Ese libro explica en su epígrafe las hazañas y aventuras de Don Quijote de la Mancha en "
    "Kuwait. (\"That book explains in its epigraph the deeds and adventures of Don Quijote de la "
    "Mancha in Kuwait.\")\n"
    "Queda gazpacho, fibra, látex, jamón, kiwi y viñas. (\"There are still gazpacho, fibre, latex, "
    "ham, kiwi and vineyards.\")\n"
    "Whisky bueno: ¡excitad mi frágil pequeña vejez! (\"Good whisky, excite my frail, little old "
    "age!\")\n"
    "Es extraño mojar queso en la cerveza o probar whisky de garrafa. (\"It is strange to dip "
    "cheese in beer or to try a whisky out of a carafe.\")\n"
    "\nSwedish:\n"
    "Flygande bäckasiner söka hwila på mjuka tuvor. (\"Flying snipes seek rest on soft "
    "tussocks.\")\n"
    "Yxmördaren Julia Blomqvist på fäktning i Schweiz. (\"Axe killer Julia Blomqvist on fencing in "
    "Switzerland.\")\n"
    "Schweiz för lyxfjäder på qvist bakom ugn. (\"Switzerland for luxury feather on branch behind "
    "oven.\")\n"
    "FAQ om Schweiz: Klöv du trång pjäxby? (\"FAQ about Switzerland: Did you cleave a narrow "
    "village of ski boots?\")\n"
    "Yxskaftbud, ge vår WC-zonmö IQ-hjälp. (\"Axe-handle carrier, give our WC zone-maiden IQ "
    "support.\")\n"
    "Gud hjälpe Zorns mö qwickt få byx av. (\"God help Zorn's girlfriend quickly get her pants "
    "off.\")\n"
    "Byxfjärmat föl gick på duvshowen. (\"Foal without pants went to the dove show.\")\n"
    "\nTurkish:\n"
    "Pijamalı hasta yağız şoföre çabucak güvendi. (\"The sick person in pyjamas quickly trusted "
    "the "
    "swarthy driver.\")\n",

    "Ligatures:\n\n"
    "f, and f: affa\n"
    "f, and i: afia\n"
    "f, and l: afla\n"
    "f, f, and i: affia\n"
    "f, f, and l: affla\n"
    "i, and j: aija\n"
    "I, and J: AIJA\n"
    "<, and <: a<<a\n"
    ">, and >: a>>a\n"
    "?, and ': aa?'a\n"
    "!, and ': aa!'a\n"
    "‘, and ‘: a‘‘a\n"
    "’, and ’: a’’a\n"
    ", and ,: a,,a\n"
    "-, and -: a--a\n"
    "-, -, and -: a---a\n"
    //  clang-format on
};
