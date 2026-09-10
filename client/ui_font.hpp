#pragma once
// PixelPurl's visible glyphs are smaller than its nominal character size.
inline constexpr unsigned uiFontSize(unsigned size) {return (size*3+1)/2;}
