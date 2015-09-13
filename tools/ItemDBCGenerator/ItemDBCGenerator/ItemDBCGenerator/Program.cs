using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Collections;

namespace ItemDBCGenerator
{
    class Program
    {
        private static Boolean debug = false;

        private static UInt32[] ARMOR_RANGE         = { 1000000, 1717145 };
        private static UInt32[] WEAPON_AXE_RANGE = { 1717146, 2434291 };
        private static UInt32[] WEAPON_AXE_2H_RANGE = { 2434292, 3151437 };
        private static UInt32[] WEAPON_BOW_RANGE = { 3151438, 3868583 };
        private static UInt32[] WEAPON_GUN_RANGE = { 3868584, 4585729 };
        private static UInt32[] WEAPON_MACE_RANGE = { 4585730, 5302875 };
        private static UInt32[] WEAPON_MACE_2H_RANGE = { 5302876, 6020021 };
        private static UInt32[] WEAPON_POLEARM_RANGE = { 6020022, 6737167 };
        private static UInt32[] WEAPON_SWORD_RANGE = { 6737168, 7454313 };
        private static UInt32[] WEAPON_SWORD_2H_RANGE = { 7454314, 8171459 };
        private static UInt32[] WEAPON_STAFF_RANGE = { 8888606, 9605751 };
        private static UInt32[] WEAPON_FIST_RANGE = { 11040044, 11757189 };
        private static UInt32[] WEAPON_DAGGER_RANGE = { 12474336, 13191481 };
        private static UInt32[] WEAPON_THROWN_RANGE = { 13191482, 13908627 };
        private static ItemRange[] RANGES = {
            new ItemRange(ARMOR_RANGE, Constants.ItemClass.ARMOR, Constants.ArmorSubclass.CLOTH),
            new ItemRange(WEAPON_AXE_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.AXE_ONEH),
            new ItemRange(WEAPON_AXE_2H_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.AXE_TWOH),
            new ItemRange(WEAPON_BOW_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.BOW),
            new ItemRange(WEAPON_GUN_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.GUN),
            new ItemRange(WEAPON_MACE_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.MACE_1H),
            new ItemRange(WEAPON_MACE_2H_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.MACE_2H),
            new ItemRange(WEAPON_POLEARM_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.POLEARM),
            new ItemRange(WEAPON_SWORD_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.SWORD_1H),
            new ItemRange(WEAPON_SWORD_2H_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.SWORD_2H),
            new ItemRange(WEAPON_STAFF_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.STAFF),
            new ItemRange(WEAPON_FIST_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.FIST_WEAPON),
            new ItemRange(WEAPON_DAGGER_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.DAGGER),
            new ItemRange(WEAPON_THROWN_RANGE, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.THROWN)
        };

        public static void print(String message, params Object[] arguments)
        {
            Console.WriteLine(String.Format(message, arguments));
        }

        static void Main(string[] args)
        {
            try
            {
                print("Reading config...");
                Config config = new Config();
                config.loadFile();

                print("Creating MySQL connection...");
                MySQL mySQL = new MySQL(config);

                Item dbc = new Item();
                print("Loaded Item.dbc, records: {0}", dbc.header.RecordCount);

                foreach (ItemRange range in RANGES)
                {
                    Constants.WeaponSubclass weapon = range.weaponSubClass;
                    String type = "";
                    if (weapon != Constants.WeaponSubclass.NULL)
                        type = weapon.ToString();
                    else
                        type = range.armorSubClass.ToString();
                    UInt32[] r = range.range;
                    print("{0} type {1} has range:\t\t{2} - {3}", range.itemClass.ToString(), type, r[0], r[1]);
                }

                List<Item.DBC_Record> newRecords = new List<Item.DBC_Record>();
                Random rand = new Random();
                foreach (ItemRange range in RANGES)
                {
                    int subClass = 0;
                    if (range.itemClass == Constants.ItemClass.ARMOR)
                        subClass = (int)range.armorSubClass;
                    else
                        subClass = (int)range.weaponSubClass;
                    print("Generating: {0}, {1}, {2}...", range.itemClass.ToString(), range.weaponSubClass.ToString(), range.armorSubClass.ToString());
                    String query = String.Format(@"
                        SELECT entry,class,subclass,SoundOverrideSubclass,displayid,inventoryType,Material,sheath FROM item_template WHERE
	                        class = '{0}' AND
                            subclass = '{1}' AND
	                        itemlevel < '{2}' AND
	                        itemlevel > '{3}' AND
	                        quality < '{4}' AND
                            quality > '{5}'
	                        ORDER BY entry;",
                            (int)range.itemClass, subClass, 58, 10, 4, 1);
                    UInt32[] ranges = range.range;
                    var resultSet = mySQL.query(query).Rows;
                    int count = resultSet.Count - 1;
                    for (uint i = ranges[0]; i < ranges[1]; ++i)
                    {
                        var row = resultSet[rand.Next(0, count)];
                        Item.DBC_Record record = new Item.DBC_Record();
                        record.ID = i;
                        record.itemClass = UInt32.Parse(row[1].ToString());
                        record.itemSubClass = UInt32.Parse(row[2].ToString());
                        record.sound_override_subclassid = Int32.Parse(row[3].ToString());
                        record.itemDisplayInfo = UInt32.Parse(row[4].ToString());
                        record.inventorySlotID = UInt32.Parse(row[5].ToString());
                        record.materialID = Int32.Parse(row[6].ToString());
                        record.sheathID = Int32.Parse(row[7].ToString());
                        newRecords.Add(record);
                        if (debug)
                            print("[{0},\t{1},\t{2},\t{3},\t{4},\t{5},\t{6},\t{7},\t{8}]", record.ID, record.itemClass,
                                record.itemSubClass, record.sound_override_subclassid, record.itemDisplayInfo,
                                record.inventorySlotID, record.materialID, record.sheathID);
                    }
                }

                print("Generated {0} new records.", newRecords.Count);

                print("Saving new Item.dbc...");
                dbc.SaveDBCFile(newRecords.ToArray());

                print("Done. Program terminating.");
            }
            catch (Exception e)
            {
                print("ERROR: {0}", e.Message);
            }
        }
    }
}
