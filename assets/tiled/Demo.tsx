<?xml version='1.0' encoding='UTF-8'?>
<tileset version="1.10" tiledversion="1.11.2" name="Demo" tilewidth="256" tileheight="512" tilecount="11" columns="0">
  <grid orientation="isometric" width="1" height="1" />
  <tile id="12">
    <image source="Sprites/crate_E.png" width="256" height="512" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="64,448 128,416 192,448 128,480" />
      </object>
    </objectgroup>
  </tile>
  <tile id="13">
    <image source="Sprites/doorway_E.png" width="256" height="512" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="0,448 32,432 64,448 32,464" />
      </object>
      <object id="2" name="Solid footprint" x="0" y="0">
        <polygon points="96,400 128,384 160,400 128,416" />
      </object>
    </objectgroup>
  </tile>
  <tile id="14">
    <image source="Sprites/fence_W.png" width="256" height="512" />
    <objectgroup draworder="index" id="7">
      <object id="6" x="127.574" y="510.977">
        <polygon points="0,0 -9.55097,-6.13991 120.751,-73.6789 128.938,-66.1746" />
      </object>
    </objectgroup>
  </tile>
  <tile id="15">
    <image source="Sprites/floor_E.png" width="256" height="512" />
    <properties>
      <property name="walkable" type="bool" value="true" />
    </properties>
  </tile>
  <tile id="16">
    <image source="Sprites/stairs_E.png" width="256" height="512" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="0,448 128,384 256,448 128,512" />
      </object>
    </objectgroup>
  </tile>
  <tile id="17">
    <image source="Sprites/stairs_S.png" width="256" height="512" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="0,448 128,384 256,448 128,512" />
      </object>
    </objectgroup>
  </tile>
  <tile id="18">
    <image source="Sprites/stairsCornerOuter_S.png" width="256" height="512" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="0,448 128,384 256,448 128,512" />
      </object>
    </objectgroup>
  </tile>
  <tile id="19">
    <image source="Sprites/switchFloorOn_E.png" width="256" height="512" />
    <properties>
      <property name="walkable" type="bool" value="true" />
    </properties>
    <objectgroup draworder="index">
      <object id="1" name="Floor switch damage" class="DamageTrigger" x="0" y="0">
        <polygon points="64,448 128,416 192,448 128,480" />
      </object>
    </objectgroup>
  </tile>
  <tile id="20">
    <image source="Sprites/wall_E.png" width="256" height="512" />
    <objectgroup draworder="index" id="2">
      <object id="1" x="-0.682212" y="447.531">
        <polygon points="0,0 128.256,-71.6322 160.32,-50.4837 32.064,15.6909" />
      </object>
    </objectgroup>
  </tile>
  <tile id="21">
    <image source="Sprites/wallCurve_S.png" width="256" height="512" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="0,448 48,427 64,443 32,464" />
      </object>
      <object id="2" name="Solid footprint" x="0" y="0">
        <polygon points="48,427 128,412 128,432 64,443" />
      </object>
      <object id="3" name="Solid footprint" x="0" y="0">
        <polygon points="128,412 208,427 192,443 128,432" />
      </object>
      <object id="4" name="Solid footprint" x="0" y="0">
        <polygon points="208,427 256,448 224,464 192,443" />
      </object>
    </objectgroup>
  </tile>
  <tile id="22">
    <image source="Sprites/window_S.png" width="256" height="512" />
    <objectgroup draworder="index">
      <object id="1" name="Solid footprint" x="0" y="0">
        <polygon points="96,384 256,464 224,480 96,416" />
      </object>
    </objectgroup>
  </tile>
</tileset>