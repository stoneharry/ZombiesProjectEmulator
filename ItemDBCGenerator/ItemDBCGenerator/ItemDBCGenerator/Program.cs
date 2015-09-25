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

        private static ItemRange[] RANGES = {
            new ItemRange(new UInt32[] { 1000000, 1830378 }, Constants.ItemClass.ARMOR, Constants.ArmorSubclass.MISC),
            new ItemRange(new UInt32[] { 1830379, 2660757 }, Constants.ItemClass.ARMOR, Constants.ArmorSubclass.CLOTH),
            new ItemRange(new UInt32[] { 2660758, 3491136 }, Constants.ItemClass.ARMOR, Constants.ArmorSubclass.LEATHER),
            new ItemRange(new UInt32[] { 3491137, 4321515 }, Constants.ItemClass.ARMOR, Constants.ArmorSubclass.MAIL),
            new ItemRange(new UInt32[] { 4321516, 5151894 }, Constants.ItemClass.ARMOR, Constants.ArmorSubclass.PLATE),
            new ItemRange(new UInt32[] { 5151895, 5982273 }, Constants.ItemClass.ARMOR, Constants.ArmorSubclass.SHIELD),
            new ItemRange(new UInt32[] { 5982274, 6812652 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.AXE_ONEH),
            new ItemRange(new UInt32[] { 6812653, 7643031 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.AXE_TWOH),
            new ItemRange(new UInt32[] { 7643032, 8473410 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.BOW),
            new ItemRange(new UInt32[] { 8473411, 9303789 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.GUN),
            new ItemRange(new UInt32[] { 9303790, 10134168 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.MACE_1H),
            new ItemRange(new UInt32[] { 10134169, 10964547 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.MACE_2H),
            new ItemRange(new UInt32[] { 10964548, 11794926 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.POLEARM),
            new ItemRange(new UInt32[] { 11794927, 12625305 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.SWORD_1H),
            new ItemRange(new UInt32[] { 12625306, 13455684 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.SWORD_2H),
            new ItemRange(new UInt32[] { 13455685, 14286063 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.STAFF),
            new ItemRange(new UInt32[] { 14286064, 15116442}, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.FIST_WEAPON),
            new ItemRange(new UInt32[] { 15116443, 15946821 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.DAGGER),
            new ItemRange(new UInt32[] { 15946822, 16777200 }, Constants.ItemClass.WEAPON, Constants.WeaponSubclass.CROSSBOW)
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
