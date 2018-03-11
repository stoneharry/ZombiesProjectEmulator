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
        public Constants.InventoryType inventoryType { get; set; }
        public bool containsInventoryType { get; } = false;

        public ItemRange(UInt32[] range, Constants.ItemClass itemClass, Constants.ArmorSubclass armorSubClass)
        {
            this.range = range;
            this.itemClass = itemClass;
            this.armorSubClass = armorSubClass;
            weaponSubClass = Constants.WeaponSubclass.NULL;
        }

        public ItemRange(UInt32[] range, Constants.ItemClass itemClass, Constants.ArmorSubclass armorSubClass, Constants.InventoryType inventoryType)
        {
            this.range = range;
            this.itemClass = itemClass;
            this.armorSubClass = armorSubClass;
            this.inventoryType = inventoryType;
            weaponSubClass = Constants.WeaponSubclass.NULL;
            containsInventoryType = true;
        }

        public ItemRange(UInt32[] range, Constants.ItemClass itemClass, Constants.WeaponSubclass weaponSubClass)
        {
            this.range = range;
            this.itemClass = itemClass;
            this.weaponSubClass = weaponSubClass;
            armorSubClass = Constants.ArmorSubclass.NULL;
        }
    }
}
