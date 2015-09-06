using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ItemDBCGenerator
{
    class Item
    {
        public struct DBC_Header
        {
            public UInt32 Magic;
            public UInt32 RecordCount;
            public UInt32 FieldCount;
            public UInt32 RecordSize;
            public Int32 StringBlockSize;
        };

        // Begin DBCs
        public DBC_Header header;
        public DBC_Map body;
        // End DBCs

        public Item()
        {
            if (!File.Exists("Item.dbc"))
                throw new Exception("Item.dbc does not exist.");

            FileStream fileStream = new FileStream("Item.dbc", FileMode.Open);
            int count = Marshal.SizeOf(typeof(DBC_Header));
            byte[] readBuffer = new byte[count];
            BinaryReader reader = new BinaryReader(fileStream);
            readBuffer = reader.ReadBytes(count);
            GCHandle handle = GCHandle.Alloc(readBuffer, GCHandleType.Pinned);
            header = (DBC_Header)Marshal.PtrToStructure(handle.AddrOfPinnedObject(), typeof(DBC_Header));
            handle.Free();

            count = Marshal.SizeOf(typeof(DBC_Record));
            if (header.RecordSize != count)
                throw new Exception("This DBC version is not supported! It is not 3.3.5a.");

            body.records = new DBC_Record[header.RecordCount];

            for (UInt32 i = 0; i < header.RecordCount; ++i)
            {
                count = Marshal.SizeOf(typeof(DBC_Record));
                readBuffer = new byte[count];
                reader = new BinaryReader(fileStream);
                readBuffer = reader.ReadBytes(count);
                handle = GCHandle.Alloc(readBuffer, GCHandleType.Pinned);
                body.records[i] = (DBC_Record)Marshal.PtrToStructure(handle.AddrOfPinnedObject(), typeof(DBC_Record));
                handle.Free();
            }

            body.StringBlock = Encoding.UTF8.GetString(reader.ReadBytes(header.StringBlockSize));

            reader.Close();
            fileStream.Close();
        }

        public void SaveDBCFile(DBC_Record[] newRecords)
        {
            UInt32 oldSize = header.RecordCount;
            header.RecordCount += (UInt32)newRecords.Length;

            String path = "Export/Item.dbc";

            Directory.CreateDirectory(Path.GetDirectoryName(path));
            if (File.Exists(path))
                File.Delete(path);
            FileStream fileStream = new FileStream(path, FileMode.Create);
            BinaryWriter writer = new BinaryWriter(fileStream);
            int count = Marshal.SizeOf(typeof(DBC_Header));
            byte[] buffer = new byte[count];
            GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            Marshal.StructureToPtr(header, handle.AddrOfPinnedObject(), true);
            writer.Write(buffer, 0, count);
            handle.Free();

            for (UInt32 i = 0; i < oldSize; ++i)
            {
                count = Marshal.SizeOf(typeof(DBC_Record));
                buffer = new byte[count];
                handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
                Marshal.StructureToPtr(body.records[i], handle.AddrOfPinnedObject(), true);
                writer.Write(buffer, 0, count);
                handle.Free();
            }
            for (UInt32 i = 0; i < newRecords.Length; ++i)
            {
                count = Marshal.SizeOf(typeof(DBC_Record));
                buffer = new byte[count];
                handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
                Marshal.StructureToPtr(newRecords[i], handle.AddrOfPinnedObject(), true);
                writer.Write(buffer, 0, count);
                handle.Free();
            }

            writer.Write(Encoding.UTF8.GetBytes(body.StringBlock));

            writer.Close();
            fileStream.Close();
        }

        public struct DBC_Map
        {
            public DBC_Record[] records;
            public String StringBlock;
        };

        public struct DBC_Record
        {
            public UInt32 ID;
            public UInt32 itemClass;
            public UInt32 itemSubClass;
            public Int32 sound_override_subclassid;
            public Int32 materialID;
            public UInt32 itemDisplayInfo;
            public UInt32 inventorySlotID;
            public Int32 sheathID;
        };
    }
}
