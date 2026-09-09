uniform sampler2D texture;
uniform sampler2D normalMap;
uniform vec3 sunPosition;
uniform vec3 flip;
uniform float ambient;
uniform float intensity;
uniform int lightCount;
uniform sampler2D shadowMap;
uniform bool hasShadows;
uniform sampler2D heightMap;
uniform bool hasHeight;
uniform sampler2D stairMap;
uniform int stairCount;
uniform int receiverStair;
uniform vec2 shadowOrigin;
uniform vec2 shadowSize;
uniform vec4 localLights[96]; // local X/Y, radius, height
uniform vec4 lightColors[96]; // RGB, strength
uniform vec2 lightShapes[96]; // directional contribution, falloff exponent
varying vec2 tilePosition;
vec3 stairCoordinates(vec2 ground,float height,vec2 origin,float flags) {
    vec2 pixel=ground-vec2(0.0,height)-origin;
    // Undo the same Tiled transforms used for the sprite and height texture.
    if(mod(floor(flags/8.0),2.0)>0.5)pixel.x=256.0-pixel.x;
    if(mod(floor(flags/4.0),2.0)>0.5)pixel.y=512.0-pixel.y;
    if(mod(floor(flags/2.0),2.0)>0.5)pixel=vec2(pixel.y*0.5,pixel.x*2.0);
    pixel.y+=height;
    return vec3(pixel.y-384.0+0.5*(pixel.x-128.0),pixel.y-384.0-0.5*(pixel.x-128.0),height);
}
bool intersectsBox(vec3 start,vec3 end,vec3 low,vec3 high) {
    vec3 direction=end-start;
    float enter=0.001,leave=0.999;
    for(int axis=0;axis<3;++axis) {
        if(abs(direction[axis])<0.00001) {
            if(start[axis]<=low[axis] || start[axis]>=high[axis])return false;
        } else {
            float a=(low[axis]-start[axis])/direction[axis];
            float b=(high[axis]-start[axis])/direction[axis];
            enter=max(enter,min(a,b));leave=min(leave,max(a,b));
            if(enter>=leave)return false;
        }
    }
    return true;
}
void main() {
    vec2 uv=gl_TexCoord[0].xy;
    vec4 base=texture2D(texture,uv);
    vec3 n=texture2D(normalMap,uv).rgb*2.0-1.0;
    vec3 sourceNormal=n;
    if(flip.z>0.5)n.xy=n.yx;
    if(flip.x>0.5)n.x=-n.x;
    if(flip.y>0.5)n.y=-n.y;
    vec3 direction=normalize(sunPosition-vec3(tilePosition,0.0));
    float light=ambient+intensity*max(dot(normalize(n),direction),0.0);
    float elevation=hasHeight?texture2D(heightMap,uv).r*510.0:0.0;
    if(receiverStair>0) {
        vec2 pixel=uv*vec2(256.0,512.0);
        if(receiverStair==3) {
            float q=pixel.y+0.5*abs(pixel.x-128.0)-304.0;
            float step=clamp(floor(q/52.0),0.0,3.0);
            elevation=80.0-step*20.0-(sourceNormal.y<0.0?0.0:max(0.0,q-step*52.0-32.0));
        } else {
            float x=receiverStair==2?256.0-pixel.x:pixel.x;
            float q=pixel.y+0.5*x-368.0;
            float step=clamp(floor(q/52.0),0.0,3.0);
            bool side=receiverStair==1?sourceNormal.x<0.0:sourceNormal.x>0.0;
            elevation=sourceNormal.y<0.0?80.0-step*20.0:side?512.0-0.5*abs(pixel.x-128.0)-pixel.y:
                80.0-step*20.0-max(0.0,q-step*52.0-32.0);
        }
        elevation=max(elevation,0.0);
    }
    vec2 ground=tilePosition+vec2(0.0,elevation);
    vec3 illumination=vec3(light);
    for(int i=0;i<96;++i) {
        if(i>=lightCount)break;
        vec2 delta=localLights[i].xy-ground;
        float distanceRatio=length(delta)/localLights[i].z;
        // Gaussian core with a long faint tail. Keep the boundary taper
        // outside the exponent so even low values reach zero smoothly.
        float gaussian=exp(-3.0*max(lightShapes[i].y,0.1)*distanceRatio*distanceRatio);
        float edge=1.0-smoothstep(0.6,1.0,distanceRatio);
        float falloff=gaussian*edge;
        if(falloff<=0.0)continue;
        bool blocked=false;
        if(hasShadows) {
            // March from the receiver toward the light in ground coordinates.
            // Each column stores the bottom and top of solid geometry.
            // Move the ray off its own rasterized surface. Height quantization
            // and four-pixel columns otherwise make adjacent riser pixels hit
            // alternating cells of the very stair they belong to.
            vec2 outward=vec2(n.x,n.y+1.41421356*n.z);
            float outwardLength=length(outward);
            vec2 start=ground;
            if(outwardLength>0.1)start+=outward/outwardLength*8.0;
            float startHeight=elevation+max(dot(normalize(n),vec3(0.0,-0.81649658,0.57735027)),0.0)*4.0;
            vec2 shadowDelta=localLights[i].xy-start;
            float distance=length(shadowDelta);
            for(int stair=0;stair<stairCount;++stair) {
                float u=(float(stair)+0.5)/float(stairCount);
                vec4 encoded=texture2D(stairMap,vec2(u,0.25));
                vec2 origin=shadowOrigin+vec2(encoded.r*65280.0+encoded.g*255.0,encoded.b*65280.0+encoded.a*255.0)/65535.0*shadowSize;
                vec2 kindFlags=floor(texture2D(stairMap,vec2(u,0.75)).rg*255.0+0.5);
                vec3 a=stairCoordinates(start,startHeight,origin,kindFlags.y);
                vec3 b=stairCoordinates(localLights[i].xy,localLights[i].w,origin,kindFlags.y);
                for(int step=0;step<4;++step) {
                    float edge=float(step)*32.0;
                    vec3 low=vec3(0.0),high=vec3(128.0,128.0,80.0-float(step)*20.0);
                    if(kindFlags.x<1.5){low.x=edge;high.x=edge+32.0;}
                    else if(kindFlags.x<2.5){low.y=edge;high.y=edge+32.0;}
                    else {high.xy=vec2(edge+32.0);}
                    // Subpixel inset handles encoded texture height rounding.
                    if(intersectsBox(a,b,low+vec3(0.5),high-vec3(0.5))) {blocked=true;break;}
                }
                if(blocked)break;
            }

            for(int step=1;step<128;++step) {
                if(blocked)break;
                float t=float(step)*max(4.0,distance/127.0)/max(distance,0.001);
                if(t>=1.0)break;
                vec2 point=start+shadowDelta*t;
                vec2 fieldUV=(point-shadowOrigin)/shadowSize;
                if(fieldUV.x<0.0 || fieldUV.y<0.0 || fieldUV.x>=1.0 || fieldUV.y>=1.0)continue;
                vec4 column=texture2D(shadowMap,fieldUV);
                float rayHeight=mix(startHeight,localLights[i].w,t);
                if(column.a>0.5 && rayHeight>column.r*510.0+3.0 && rayHeight<column.g*510.0-3.0) {
                    blocked=true;break;
                }
            }
        }
        if(blocked)continue;
        float dh=localLights[i].w-elevation;
        vec3 incoming=normalize(vec3(delta.x,delta.y-dh,1.41421356*delta.y+0.70710678*dh));
        float diffuse=max(dot(normalize(n),incoming),0.0);
        illumination+=lightColors[i].rgb*lightColors[i].a*falloff*mix(1.0,diffuse,lightShapes[i].x);
    }
    gl_FragColor=vec4(base.rgb*min(illumination,vec3(1.0)),base.a*gl_Color.a);
}
