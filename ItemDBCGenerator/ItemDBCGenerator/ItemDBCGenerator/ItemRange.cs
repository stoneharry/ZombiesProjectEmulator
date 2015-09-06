using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ItemDBCGenerator
{
    class ItemRange
    {
        public UInt32[] range { get; set; }
        public Constants.ItemClass itemClass { get; set; }
        public Constants.WeaponSubclass weaponSubClass { get; set; }
        public Constants.ArmorSubclass armorSubClass { get; set; }

        public ItemRange(UInt32[] range, Constants.ItemClass itemClass)
        {
            this.range = range;
            this.itemClass = itemClass;
            this.armorSubClass = Constants.ArmorSubclass.CLOTH; // TODO
            this.weaponSubClass = Constants.WeaponSubclass.NULL;
        }

        public ItemRange(UInt32[] range, Constants.ItemClass itemClass, Constants.WeaponSubclass weaponSubClass)
        {
            this.range = range;
            this.itemClass = itemClass;
            this.weaponSubClass = weaponSubClass;
            this.armorSubClass = Constants.ArmorSubclass.NULL;
        }
    }
}
