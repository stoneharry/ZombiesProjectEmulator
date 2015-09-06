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

enum MemoryBind
{
    BIND_ITEM, // memory manages the item template with the item it is attached to
    BIND_LOOT, // memory manages the item template with loot it was generated for
};

struct VirtualItemTemplate : ItemTemplate
{
    VirtualItemTemplate(ItemTemplate const* base, MemoryBind binding) : ItemTemplate(*base), base_entry(base->ItemId), itemguidlow(0), memoryBinding(binding)
    {
        FlagsCu &= ~ITEM_FLAGS_CU_VIRTUAL_ITEM_BASE; // virtual item should not be revirtualized?
        Bonding = BIND_WHEN_PICKED_UP; // All items MUST be bound on pickup so they can not be mailed and thus failing some cleanup and memory management
    }
    uint32 base_entry;
    uint32 itemguidlow;
    MemoryBind memoryBinding;
};

struct VirtualModifier
{
    VirtualModifier();
    uint8 ilevel;
    uint8 quality;
    int16 statpool;
    StatGroup statgroup;

    class StatGroupData
    {
    public:
        StatGroupData();
        std::vector<ItemModType> const& GetStatGroupStats(StatGroup group) const;
        std::vector<SocketColor> const& GetStatGroupSockets(StatGroup group) const;
        std::vector<StatGroup> const& GetArmorSubclassStatGroups(ItemSubclassArmor subclass) const;
    private:
        std::vector<ItemModType> stat_group_stats[STAT_GROUP_COUNT];
        std::vector<SocketColor> stat_group_sockets[STAT_GROUP_COUNT];
        std::vector<StatGroup> armor_type_stat_groups[MAX_ITEM_SUBCLASS_ARMOR];
    };
    static StatGroupData const premadeStatGroupData;

    static float GetSlotStatModifier(InventoryType invtype);
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

    static const uint32 minEntry = 1000000;
    static const uint32 maxEntry = 0xFFFFFF;
    static_assert(minEntry < maxEntry, "Min entry must be smaller than max entry");

    static VirtualItemMgr& instance();

    VirtualItemMgr();
    ~VirtualItemMgr();

    // does not directly remove the item. Removing delayed until actual delete
    void Remove(uint32 entry);
    // not thread safe
    void RemoveExcludedItemBound(std::set<uint32> not_removed_entries);

    bool HasSpaceFor(uint32 amount);
    VirtualItemTemplate const* GetVirtualTemplate(uint32 entry);
    VirtualItemTemplate* GenerateVirtualTemplate(ItemTemplate const* base, MemoryBind binding, VirtualModifier const& modifier = VirtualModifier());
    void GenerateStats(ItemTemplate* output, VirtualModifier const& modifier = VirtualModifier()) const;
    void SetVirtualTemplateMemoryBind(uint32 entry, MemoryBind binding);
    static bool IsVirtualTemplate(ItemTemplate const* base);

private:

    class EntryGenerator
    {
        friend class VirtualItemMgr;
    public:
        uint32 GenerateEntry(Store const& store);
        uint32 PeekNext() const;

    private:
        EntryGenerator();
        EntryGenerator(uint32 min, uint32 max);
        bool CanAllocate(uint32 amount, Store const& store) const;

        uint32 nextEntry;
        uint32 minEntry;
        uint32 maxEntry;
    };

    LockType lock;
    Store store;
    EntryGenerator armorGenerator;
    EntryGenerator weaponGenerator[MAX_ITEM_SUBCLASS_WEAPON];
    std::vector<uint32> freed_entries;

    EntryGenerator& Generator(ItemTemplate* temp);
    // used to insert virtual items on load, not thread safe
    bool InsertEntry(VirtualItemTemplate* virtualItem);
    // used to free item entries, not thread safe
    void ClearFreedEntries();
};

#define sVirtualItemMgr VirtualItemMgr::instance()

#endif
