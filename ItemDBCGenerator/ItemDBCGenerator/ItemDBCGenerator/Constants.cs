using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ItemDBCGenerator
{
    public class Constants
    {
        public enum ItemClass
        {
            CONSUMABLE          = 0,
            CONTAINER           = 1,
            WEAPON              = 2,
            GEM                 = 3,
            ARMOR               = 4,
            REAGENT             = 5,
            PROJECTILE          = 6,
            TRADE_GOODS         = 7,
            GENERIC_OBSOLETE    = 8,
            RECIPE              = 9,
            MONEY_OBSOLETE      = 10,
            QUIVER              = 11,
            QUEST               = 12,
            KEY                 = 13,
            PERMANENT_OBSOLETE  = 14,
            MISCALLANEOUS       = 15,
            GLYPH               = 16
        }

        public enum WeaponSubclass
        {
            NULL                = -1,
            AXE_ONEH            = 0,
            AXE_TWOH            = 1,
            BOW                 = 2,
            GUN                 = 3,
            MACE_1H             = 4,
            MACE_2H             = 5,
            POLEARM             = 6,
            SWORD_1H            = 7,
            SWORD_2H            = 8,
            OBSOLETE            = 9,
            STAFF               = 10,
            EXOTIC_1            = 11,
            EXOTIC_2            = 12,
            FIST_WEAPON         = 13,
            MISC                = 14,
            DAGGER              = 15,
            THROWN              = 16,
            SPEAR               = 17,
            CROSSBOW            = 18,
            WAND                = 19,
            FISHING_POLE        = 20
        }

        public enum ArmorSubclass
        {
            NULL                = -1,
            MISC                = 0,
            CLOTH               = 1,
            LEATHER             = 2,
            MAIL                = 3,
            PLATE               = 4,
            BUCKLER_OBSOLETE    = 5,
            SHIELD              = 6,
            LIBRAM              = 7,
            IDOL                = 8,
            TOTEM               = 9,
            SIGIL               = 10
        }
    }
}
