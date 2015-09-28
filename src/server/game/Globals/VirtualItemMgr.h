#include "Define.h" // uint32 etc types
#include "ItemPrototype.h" // Item enums
#include "DBCStores.h" // Needed for item DBC lookup
#include <map>
#include <set>
#include <vector>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#ifndef VIRTUAL_ITEM_MGR_H
#define VIRTUAL_ITEM_MGR_H

class ObjectMgr;
class World;

enum StatGroup
{
    STAT_GROUP_HEALING,
    STAT_GROUP_INT_DPS,
    STAT_GROUP_STR_DPS,
    STAT_GROUP_STR_TANK,
    STAT_GROUP_AGI_DPS,
    STAT_GROUP_AGI_TANK,
    STAT_GROUP_AGI_RANGED,
    STAT_GROUP_COUNT,
    STAT_GROUP_RANDOM = STAT_GROUP_COUNT,
};

struct VirtualItemTemplate : ItemTemplate
{
    VirtualItemTemplate(ItemTemplate const* base) : ItemTemplate(*base), base_entry(base->ItemId)
    {
        FlagsCu &= ~ITEM_FLAGS_CU_VIRTUAL_ITEM_BASE; // virtual item should not be revirtualized?
        //Bonding = BIND_WHEN_PICKED_UP; // All items MUST be bound on pickup so they can not be mailed and thus failing some cleanup and memory management
    }

    /**
     * Entry of the original item used to generate the template.
     */
    uint32 base_entry;

    /**
     * Updates the displayId used by the item from DBC data to match the current item entry's displayid.
     */
    void UpdateDisplay();
};

struct VirtualModifier
{
    VirtualModifier();

    /**
     * Different modifiers that can be edited to change the output when the modifier is used to generate stats.
     */
    uint8 ilevel;
    uint8 quality;
    int16 statpool;
    StatGroup statgroup;

    class StatGroupData
    {
    public:
        /**
         * Generates the premade stat groups and similar in addition to constructing the object itself.
         */
        StatGroupData();

        /**
         * Returns the stats for the given stat group.
         */
        std::vector<ItemModType> const& GetStatGroupStats(StatGroup group) const;

        /**
         * Returns the sockets for the given stat group.
         */
        std::vector<SocketColor> const& GetStatGroupSockets(StatGroup group) const;

        /**
         * Returns the stat groups for the given armor subclass.
         */
        std::vector<StatGroup> const& GetArmorSubclassStatGroups(ItemSubclassArmor subclass) const;
    private:
        std::vector<ItemModType> stat_group_stats[STAT_GROUP_COUNT];
        std::vector<SocketColor> stat_group_sockets[STAT_GROUP_COUNT];
        std::vector<StatGroup> armor_type_stat_groups[MAX_ITEM_SUBCLASS_ARMOR];
    };

    /**
     * A static object used to fetch different predefined stat groups and similar.
     * The object has various methods to access these lists.
     */
    static StatGroupData const premadeStatGroupData;

    /**
     * Fetches the rate (point*rate = stat_amount) for the given item equip type.
     * Returns the stat rate.
     */
    static float GetSlotStatModifier(InventoryType invtype);

    /**
     * Fetches the rate (point*rate = stat_amount) for the given stat type.
     * Returns the stat rate.
     */
    static float GetStatRate(ItemModType stat);
};

class VirtualItemMgr
{
    friend class ObjectMgr;
    friend class World;
public:
    typedef std::unordered_map<uint32, VirtualItemTemplate*> Store;

    typedef boost::shared_mutex LockType;
    typedef boost::shared_lock<LockType> ReadGuard;
    typedef boost::unique_lock<LockType> WriteGuard;

    /**
     * minEntry and maxEntry are used as the boundaries for the entries the EntryGenerators use.
     * The whole range defined by minEntry and maxEntry can be divided for the EntryGenerators created in VirtualItemMgr.
     * All virtual entries are assumed to be between this range.
     */
    static const uint32 minEntry = 1000000;
    static const uint32 maxEntry = 0xFFFFFF;
    static_assert(minEntry < maxEntry, "Min entry must be smaller than max entry");

    /**
     * armorSubclasses and weaponSubclasses contain the valid subclasses for items of armor and weapon.
     */
    static const std::vector<ItemSubclassArmor> armorSubclasses;
    static const std::vector<ItemSubclassWeapon> weaponSubclasses;

    /**
     * Returns a single static instance of VirtualItemMgr.
     */
    static VirtualItemMgr& instance();

	/**
	 * Not thread safe.
	 * Loads all possible names from the generator table into memory.
	 */
	void LoadNamesFromDB();

	struct NameInfo
	{
		NameInfo() {}
		NameInfo(int32 type, int32 sub, int32 arrid) : itemType(type), subclass(sub), array_id(arrid) {}
		NameInfo(int32 type, int32 sub, int32 arrid, std::string n) : itemType(type), subclass(sub), array_id(arrid), name(n) {}
		int32 itemType;
		int32 subclass;
		int32 array_id;
		std::string name;
	};

    /**
     * Returns a randomly generated item name depending on item type, subclass and quality
     */
    std::string GenerateItemName(uint32 type, uint32 subclass, uint32 quality) const;

	/**
	 * Return a vector of available names for the specified subclass.
	 */
	std::vector<std::string> GetNamesForNameInfo(NameInfo* info) const;

    /**
     * Creates all used generators and sets their entry ranges in addition to constructing the object itself.
     */
    VirtualItemMgr();

    /**
     * Deletes all stored virtual templates in addition to deleting the object itself.
     */
    ~VirtualItemMgr();

    /**
     * Fetches the VirtualItemTemplate assigned for the unique entry.
     * Returns VirtualItemTemplate if found, else returns null.
     */
    VirtualItemTemplate const* GetVirtualTemplate(uint32 entry);

    /**
     * Uses passed base and modifier to generate a new VirtualItemTemplate.
     * Returns the newly created VirtualItemTemplate.
     */
    VirtualItemTemplate* GenerateVirtualTemplate(ItemTemplate const* base, VirtualModifier const& modifier = VirtualModifier());

    /**
     * Uses passed modifier to generate stats and edits output to have the generated stats.
     */
    void GenerateStats(ItemTemplate* output, VirtualModifier const& modifier = VirtualModifier()) const;

    /**
     * Checks if the passed template is a valid virtual item template.
     * Returns true if it is, false if it is not.
     */
    static bool IsVirtualTemplate(ItemTemplate const* base);

private:

    class EntryGenerator
    {
        friend class VirtualItemMgr;
    public:
        /**
         * Not thread safe. Crashes if no entry found or entry not between assumed range.
         * Generates a new entry from the generator's range that is not used in passed container.
         * Returns unique unused entry.
         */
        uint32 GenerateEntry(Store const& store);

        /**
         * Not thread safe.
         * Checks the current entry that is to be used next.
         * Returns next unique unused entry.
         */
        uint32 PeekNext() const;

        /**
         * Generates a new EntryGenerator with entry range 0-1 by default.
         */
        EntryGenerator();

        /**
         * Crashes if min >= max.
         * Generates a new EntryGenerator with given range.
         */
        EntryGenerator(uint32 min, uint32 max);
        
    private:
        uint32 nextEntry;
        uint32 minEntry;
        uint32 maxEntry;
    };

    LockType lock;
    Store store;
    std::unordered_map<ItemSubclassArmor, EntryGenerator> armorGenerator;
    std::unordered_map<ItemSubclassWeapon, EntryGenerator> weaponGenerator;
    std::vector<uint32> freed_entries;

	std::vector<NameInfo> availableNames;

    /**
     * Not thread safe.
     * Fetches the EntryGenerator used for items with same type as passed ItemTemplate.
     * Returns EntryGenerator* if it succeeds, null if it fails.
     */
    EntryGenerator* Generator(ItemTemplate* temp);

    /**
     * Not thread safe.
     * Used to insert virtual items on startup when loading database.
     * Returns true if it succeeds and generates a new entry if needed, false if it fails.
     */
    bool InsertEntry(VirtualItemTemplate* virtualItem);
};

#define sVirtualItemMgr VirtualItemMgr::instance()

#endif
