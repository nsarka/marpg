<?xml version='1.0' encoding='UTF-8'?>
<tileset version="1.10" tiledversion="1.12.2" name="Fantasy" tilewidth="256" tileheight="256" tilecount="6" columns="0">
  <tileoffset x="-64" y="14" />
  <properties><property name="tile_layer_alignment" value="bottom_left" /></properties>
  <grid orientation="isometric" width="128" height="64" />
  <tile id="0">
    <image source="../Fantasy tileset - 2D Isometric/Environment/Ground A1_E.png" width="256" height="256" />
  </tile>
  <tile id="1">
    <image source="../Fantasy tileset - 2D Isometric/Environment/Ground A2_E.png" width="256" height="256" />
  </tile>
  <tile id="2">
    <image source="../Fantasy tileset - 2D Isometric/Environment/Tree A1_E.png" width="256" height="256" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="112,210 128,200 145,210 128,220" />
      </object>
    </objectgroup>
  </tile>
  <tile id="3">
    <image source="../Fantasy tileset - 2D Isometric/Environment/Wall A1_E.png" width="256" height="256" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="112,228 128,236 192,204 176,196" />
      </object>
    </objectgroup>
  </tile>
  <tile id="4">
    <image source="../Fantasy tileset - 2D Isometric/Environment/Wall A1_N.png" width="256" height="256" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="112,196 128,188 192,220 176,228" />
      </object>
    </objectgroup>
  </tile>
  <tile id="5">
    <image source="../Fantasy tileset - 2D Isometric/Environment/Chest A1_E.png" width="256" height="256" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="104,210 126,198 156,212 134,224" />
      </object>
    </objectgroup>
  </tile>
</tileset>