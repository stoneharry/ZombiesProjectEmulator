using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WoW.Streaming
{
    enum Offsets
    {
        MapTileArray2 = 0xCE08D0,
        MapTileArray = 0xCE48D0,

        TileMapFirst = 0xADFBF4,
        TileMapNext = 0xADFBEC
    }
}
