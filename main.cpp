#include<iostream>
#include<vector>
#include<cmath>
#include<graphics.h>
#include<easyx.h>
#include<map>
#include<set>
#include<ctime>
#include<windows.h>
#include<thread>
#include<Mmsystem.h>
#include<algorithm>
#include<fstream>
#include<filesystem>
#include<experimental/filesystem>
#include<mutex>
#include<unordered_map>
#include<conio.h>

#pragma comment(lib,"winmm.lib")

#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME)&0x121280)?1:0)
#define Pi 3.1415926535

using namespace std;

int click_all,click_valuble,fps_out;
bool space=false;
bool flag=false;
bool DRAW=0;
bool is_down=false;
bool onGround=false;
bool bclick;
bool rclick;
bool wait_jump;
bool shift;
recursive_mutex cam_mtx;
double vy=0;
const double G=0.02;
bool in_air=0;
double target_h=0;
const double J_H=1.5;
bool needUpdateFaces=true;//新增：标记是否需要更新面
double V_y=0.0;
bool last_space=false;

class point_3D;
class point_2D;
class vec;
class block;

class MultiKeyDetector{
private:
    map<int,bool>keyStates;
    
public:
    MultiKeyDetector(){
        keyStates[VK_UP]=false;
        keyStates[VK_DOWN]=false;
        keyStates[VK_LEFT]=false;
        keyStates[VK_RIGHT]=false;
        keyStates['W']=false;
        keyStates['A']=false;
        keyStates['S']=false;
        keyStates['D']=false;
        keyStates['Q']=false;
        keyStates['E']=false;
        keyStates[VK_SPACE]=false;
        keyStates[VK_SHIFT]=false;
        keyStates[VK_CONTROL]=false;
        keyStates[VK_ESCAPE]=false;
        keyStates[VK_LBUTTON]=false;
        keyStates[VK_RBUTTON]=false;
    }
    
    void updateKeyStates(){
        for(auto&pair:keyStates){
            pair.second=(GetAsyncKeyState(pair.first)&0x121280)!=0;
        }
    }
    
    bool isKeyPressed(int virtualKey){
        return keyStates[virtualKey];
    }
};

class block{
public:
    double x,y,z;
    double bc;
    block(double x=0,double y=0,double z=0,double bc=0.01):x(x),y(y),z(z),bc(bc){}
};

class point_3D{
public:
    double x,y,z;
    point_3D(double x=0,double y=0,double z=0):x(x),y(y),z(z){}
    point_3D(const point_3D&p):x(p.x),y(p.y),z(p.z){}
    point_3D(const block&p):x(p.x),y(p.y),z(p.z){}
};

bool operator<(const point_3D&a,const point_3D&b){
    if(fabs(a.x-b.x)<1e-9){
        if(fabs(a.y-b.y)<1e-9){
            return a.z<b.z;
        }
        return a.y<b.y;
    }
    return a.x<b.x;
}

class vec{
public:
    double x,y,z;//tmd，我也不知道哪个坐标是具体的哪个轴了，一坨屎能跑就行啊，别管那么多了，到时候一次一次的试就行了
    //应该是x,z,y
    vec(double x=0,double y=0,double z=0):x(x),y(y),z(z){}
    vec(const point_3D&a,const point_3D&b):x(b.x-a.x),y(b.y-a.y),z(b.z-a.z){}
    double length(){
        return sqrt(x*x+y*y+z*z);
    }
    void normalize(){
        double len=length();
        if(len>0){
            x/=len;
            y/=len;
            z/=len;
        }
    }
    friend vec operator*(const vec&a,double k){
        return vec(a.x*k,a.y*k,a.z*k);
    }
    
    friend vec operator+(const vec&a,const vec&b){
        return vec(a.x+b.x,a.y+b.y,a.z+b.z);
    }
};

struct CubeFace{
    point_3D point_cube[4];
    vec normal;
    double depth;
    int blockType;//新增：方块类型
    
    CubeFace(point_3D v0,point_3D v1,point_3D v2,point_3D v3,vec n,double d=0.0,int type=1):normal(n),depth(d),blockType(type){
        point_cube[0]=v0;
        point_cube[1]=v1;
        point_cube[2]=v2;
        point_cube[3]=v3;
    }
};
int randmod(int mod){
    return ((rand()-1)%mod+1);
}

class RandMap{
private:
    double xi[20];
    double yi[20];
    int P[512];
    double fade(double t){//平滑函数
        return t*t*t*(t*(t*6-15)+10);
    }
    double lerp(double a,double b,double t){//插值
        return a+t*(b-a);
    }
    void initG(){//G梯度
        for(int i=0;i<16;i++){
            double x=rand()*1.0/RAND_MAX*1.0;
            double y=rand()*1.0/RAND_MAX*1.0;
            double l=sqrt(x*x+y*y);
            xi[i]=x/l;
            yi[i]=y/l;
        }
        return;
    }
    void initPailiebiao(){//排列表
        for(int i=0;i<256;i++){
            P[i]=i;
        }
        for(int i=0;i<256;++i){
            int j=rand()%256;
            if(rand()&1){
                swap(P[i],P[j]);
            }
        }
        for(int i=0;i<256;i++){
            P[i+256]=P[i];
        }
        for(int i=0;i<256;i++){
            cout<<P[i]<<" ";
        }
        return;
    }
    void getG(int hash,double& Gx,double& Gy){
        int index=hash%16;
        Gx=xi[index];
        Gy=yi[index];
        return;
    }
    double Noise(double x,double y){
        int X=int(floor(x))&255;
        int Y=int(floor(y))&255;
        double xf=x-floor(x);
        double yf=y-floor(y);
        double u=fade(xf);
        double v=fade(yf);

        int H1=P[P[X]+Y];
        int H2=P[P[X]+Y+1];
        int H3=P[P[X+1]+Y];
        int H4=P[P[X+1]+Y+1];

        double x1,y1,x2,y2,x3,y3,x4,y4;
        getG(H1,x1,y1);
        getG(H2,x2,y2);
        getG(H3,x3,y3);
        getG(H4,x4,y4);

        return lerp(lerp(x1*xf+y1*yf,x3*(xf-1)+x3*yf,u),lerp(x2*xf+y2*(yf-1),x4*(xf-1)+y4*(yf-1),u),v);

    }
public:
    vector<vector<int>>mp;
    RandMap(int n,int m,int seed=rand()):mp(n,vector<int>(m,0)){
        initPailiebiao();
        initG();
        for(int i=0;i<n;++i){
            for(int j=0;j<m;++j){
                double Total=0;
                double fqc=0.125;
                double ampl=1.0;
                double maxValue=0;
                for(int k=0;k<4;k++){
                    Total+=Noise(i*fqc,j*fqc)*ampl;
                    maxValue+=ampl;
                    ampl*=0.5;
                    fqc*=2.0;
                }
                if(maxValue>0){
                    Total/=maxValue;
                }
                Total=(Total+1)/2.0;
                Total=max(0.0,min(1.0,Total));
                Total-=0.5;
                Total*=10;
                cout<<Total<<" ";
                mp[i][j]=int(0+Total*5);
            }
            
            cout<<endl;
        }

        
        int minn=1000000;
        for(int i=0;i<n;++i){
            for(int j=0;j<m;++j){
                minn=min(minn,mp[i][j]);
            }
        }
        for(int i=0;i<n;++i){
            for(int j=0;j<m;++j){
                mp[i][j]-=minn;
                mp[i][j]++;
            }
        }
    }

    
};

map<point_3D,int>Map;
map<point_3D,bool>Selected;
unordered_map<int,vector<CubeFace>>visibleFacesCache;

int currentCacheKey=0;

vec D[]={vec(0,0,0),vec(1,0,0),vec(1,1,0),vec(0,1,0),vec(0,0,1),vec(1,0,1),vec(1,1,1),vec(0,1,1)};

point_3D operator+(const point_3D&a,const vec&b){
    return point_3D(a.x+b.x,a.y+b.y,a.z+b.z);
}

double dot_product(const vec&a,const vec&b){
    return a.x*b.x+a.y*b.y+a.z*b.z;
}

vec cross_product(const vec&a,const vec&b){
    return vec(
        a.y*b.z-a.z*b.y,
        a.z*b.x-a.x*b.z,
        a.x*b.y-a.y*b.x
    );
}

class camera{
public:
    point_3D position;
    double yaw,pitch;
    double moveSpeed;
    double mouseSensitivity;
    vec front,right,up;
    camera(const point_3D&pos=point_3D(0,0,0),double yaw=-90.0,double pitch=0.0)
        :position(pos),yaw(yaw),pitch(pitch),moveSpeed(0.1),mouseSensitivity(0.1){
        updateVectors();
    }
    void updateVectors(){
        vec newFront(
            cos(yaw*Pi/180.0)*cos(pitch*Pi/180.0),
            sin(pitch*Pi/180.0),
            sin(yaw*Pi/180.0)*cos(pitch*Pi/180.0)
        );
        newFront.normalize();
        front=newFront;
        vec worldUp(0,1,0);
        right=cross_product(front,worldUp);
        right.normalize();
        up=cross_product(right,front);
        up.normalize();
    }
    
    void processMouseMovement(double xOffset,double yOffset){
        xOffset*=mouseSensitivity;
        yOffset*=mouseSensitivity;
        yaw+=xOffset;
        pitch+=yOffset;
        if(pitch>89.0)pitch=89.0;
        if(pitch<-89.0)pitch=-89.0;
        updateVectors();
    }
    void moveForward(double distance){
        position.x=position.x+front.x*distance;
        position.z=position.z+front.z*distance;
        //position=position+front*distance;
    }
    
    void moveRight(double distance){
        position=position+right*distance;
    }
    
    void moveUp(double distance){
        position=position+vec(0,1,0)*distance;
    }
};

class point_2D{
public:
    double x,y;
    point_2D(double x=0,double y=0):x(x),y(y){}
    point_2D(const point_2D&p):x(p.x),y(p.y){}
};

class Edge{
public:
    point_3D from,to;
    Edge(point_3D from,point_3D to):from(from),to(to){}
};

struct qujian{
    double l,r;
};

point_3D operator+(const block&a,const vec&b){
    return point_3D(a.x+b.x,a.y+b.y,a.z+b.z);
}

vector<point_3D>point_q;
vector<point_2D>print_q;
vector<Edge>bian;
vector<block>blocks;
camera myself;

bool cmp_FaceDep(const CubeFace&a,const CubeFace&b){
    return a.depth>b.depth;//深度大的（远的）先绘制
}

bool cmp_bDis(block a,block b){
    vec va=vec(point_3D(a),myself.position);
    vec vb=vec(point_3D(b),myself.position);
    return va.length()<vb.length();
}
//透视投影函数
point_2D perspectiveProjection(const point_3D&point,const camera&cam){
    vec viewVector(point,cam.position);
    double x=dot_product(viewVector,cam.right);
    double y=dot_product(viewVector,cam.up);
    double z=dot_product(viewVector,cam.front);
    if(z<=-0.0005)return point_2D(0,0);
    double scale=500.0/z;
    return point_2D(x*scale,y*scale);
}
point_2D perspectiveProjection_nondelete(const point_3D&point,const camera&cam){
    vec viewVector(point,cam.position);

    double x=dot_product(viewVector,cam.right);
    double y=dot_product(viewVector,cam.up);
    double z=dot_product(viewVector,cam.front);
    double scale=500.0/z;

    return point_2D(x*scale,y*scale);
}

qujian jiao(qujian a,qujian b){
    if(a.l>a.r||b.l>b.r)return{1,-1};
    if(a.r<b.l||a.l>b.r)return{1,-1};
    return{max(a.l,b.l),min(a.r,b.r)};
}

bool Check(block b){
    /*  
        我们有:x=x_myself+k*x_towards
        判断法一:对于单个分量，求出对应k区间，对所有区间求交，若非空集则有交集
                (查了一下，这个好像叫Slabs)
    */

    vec to=myself.front*(-1);
    
    double tMin=0.0;
    double tMax=1e9;
    
    //分别检查三个轴
    for(int i=0;i<3;i++){
        double invD,t1,t2;
        double rayOrigin,rayDir,boxMin,boxMax;
        
        switch(i){
            case 0:
                rayOrigin=myself.position.x;
                rayDir=to.x;
                boxMin=b.x;
                boxMax=b.x+b.bc;
                break;
            case 1:
                rayOrigin=myself.position.y;
                rayDir=to.y;
                boxMin=b.y;
                boxMax=b.y+b.bc;
                break;
            case 2:
                rayOrigin=myself.position.z;
                rayDir=to.z;
                boxMin=b.z;
                boxMax=b.z+b.bc;
                break;
        }
        
        if(fabs(rayDir)<1e-6){
            if(rayOrigin<boxMin||rayOrigin>boxMax){
                return false;
            }
        }
        else{
            invD=1.0/rayDir;
            t1=(boxMin-rayOrigin)*invD;
            t2=(boxMax-rayOrigin)*invD;
            if(t1>t2){
                double temp=t1;
                t1=t2;
                t2=temp;
            }
            
            tMin=max(tMin,t1);
            tMax=min(tMax,t2);
            
            if(tMin>tMax)return false;
        }
    }
    return tMax>=0&&tMin<=tMax;
}
int Del(){
    int count=0;
    //首先根据distance排序
    sort(blocks.begin(),blocks.end(),cmp_bDis);
    for(vector<block>::iterator it=blocks.begin();it!=blocks.end();){
        if(Check((*it))){
            //cout<<"OK";

            Map[{(int)(*it).x,(int)(*it).y,(int)(*it).z}]=0;
            //cout<<"Delete:"<<(int)(*it).x<<" "<<(int)(*it).y<<" "<<(int)(*it).z<<endl;
            blocks.erase(it);
            needUpdateFaces=true;//标记需要更新面
            count++;
            break;
        }
        else{
            it++;
        }
    }
    return count;
}
void add_block(int count){
    vec to=myself.front*(-1);
    while(count--){
        point_3D p=myself.position+to*20;
        p.x+=rand()*4.0/RAND_MAX*pow(-1,rand());
        p.y+=rand()*4.0/RAND_MAX*pow(-1,rand());
        p.z+=rand()*4.0/RAND_MAX*pow(-1,rand());
        blocks.push_back(block(p.x,p.y,p.z,abs(0.5)));
        needUpdateFaces=true;//标记需要更新面
    }
    return;
}
void set_(int &step,double &start,double &forward,double &maxm,double &delta,int &now){
    if(forward!=0){
        double next=(step>0)?(now+1):now;
        maxm=(next-start)/forward;
        delta=(step/forward);
    }
    else{
        maxm=0x3f3f3f3f;
        delta=0;
    }
    return;
}

int initsetfwd(double f){
    if(f>0)return 1;
    else return -1;
}
int setfwd(int f){
    if(f>0)return 1;
    else return -1;
}

void update_map(){//废弃函数，给正方体单独绘制边
    bian.clear();
    for(int i=1;i<=10;i++){
        for(int j=1;j<=10;++j){
            Map[{i,0,j}]=1;
        }
    }

    
    point_3D start=myself.position;
    vec fwd=myself.front*(-1);
    
    int sx,sy,sz;
    double mx,my,mz;
    double dx,dy,dz;
    
    int nx=floor(start.x);
    int ny=floor(start.y);
    int nz=floor(start.z);
    
    sx=initsetfwd(fwd.x);
    sy=initsetfwd(fwd.y);
    sz=initsetfwd(fwd.z);
    
    //分别设置x,y,z的初始值和步长
    set_(sx,start.x,fwd.x,mx,dx,nx);
    set_(sy,start.y,fwd.y,my,dy,ny);
    set_(sz,start.z,fwd.z,mz,dz,nz);
    
    int prex=nx;
    int prey=ny;
    int prez=nz;
    
    for(int i=0;i<20;i++){
        if(Map[{nx,ny,nz}]!=0){

            block blocki(nx,ny,nz,1);

            bian.push_back(Edge(blocki+D[0]*blocki.bc,blocki+D[1]*blocki.bc));
            bian.push_back(Edge(blocki+D[1]*blocki.bc,blocki+D[2]*blocki.bc));
            bian.push_back(Edge(blocki+D[2]*blocki.bc,blocki+D[3]*blocki.bc));
            bian.push_back(Edge(blocki+D[3]*blocki.bc,blocki+D[0]*blocki.bc));

            //
            bian.push_back(Edge(blocki+D[0]*blocki.bc,blocki+D[4]*blocki.bc));
            bian.push_back(Edge(blocki+D[1]*blocki.bc,blocki+D[5]*blocki.bc));
            bian.push_back(Edge(blocki+D[2]*blocki.bc,blocki+D[6]*blocki.bc));
            bian.push_back(Edge(blocki+D[3]*blocki.bc,blocki+D[7]*blocki.bc));

            //
            bian.push_back(Edge(blocki+D[4]*blocki.bc,blocki+D[5]*blocki.bc));
            bian.push_back(Edge(blocki+D[5]*blocki.bc,blocki+D[6]*blocki.bc));
            bian.push_back(Edge(blocki+D[6]*blocki.bc,blocki+D[7]*blocki.bc));
            bian.push_back(Edge(blocki+D[7]*blocki.bc,blocki+D[4]*blocki.bc));
            
            break;
        }
        
        if(mx<=my&&mx<=mz){
            nx+=sx;
            mx+=dx;
        }
        else if(my<=mx&&my<=mz){
            ny+=sy;
            my+=dy;
        }
        else{
            nz+=sz;
            mz+=dz;
        }
    }

    return;
}

bool haveB(int x,int y,int z){
    auto it=Map.find({x,y,z});
    return it!=Map.end()&&it->second!=0;
}

void updateAllVisibleFaces(){
    visibleFacesCache.clear();
    
    for(const auto&block:blocks){
        int x=block.x;
        int y=block.y;
        int z=block.z;
    
        //只添加与空气接触的面
        if(!haveB(x,y,z+1)){//前
            point_3D base(x,y,z);
            vector<CubeFace>faces;
            faces.push_back(CubeFace(
                base+vec(0,0,1),base+vec(1,0,1),
                base+vec(1,1,1),base+vec(0,1,1),
                vec(0,0,1)
            ));
            visibleFacesCache[x*10000000+y*10000+z*6]=faces;//可能出问题
        }
        if(!haveB(x,y,z-1)){//后
            point_3D base(x,y,z);
            vector<CubeFace>faces;
            faces.push_back(CubeFace(
                base+vec(0,0,0),base+vec(0,1,0),
                base+vec(1,1,0),base+vec(1,0,0),
                vec(0,0,-1)
            ));
            visibleFacesCache[x*10000000+y*10000+z*6+1]=faces;
        }
        if(!haveB(x,y+1,z)){//上
            point_3D base(x,y,z);
            vector<CubeFace>faces;
            faces.push_back(CubeFace(
                base+vec(0,1,0),base+vec(0,1,1),
                base+vec(1,1,1),base+vec(1,1,0),
                vec(0,1,0)
            ));
            visibleFacesCache[x*10000000+y*10000+z*6+2]=faces;
        }
        if(!haveB(x,y-1,z)){//下
            point_3D base(x,y,z);
            vector<CubeFace>faces;
            faces.push_back(CubeFace(
                base+vec(0,0,0),base+vec(1,0,0),
                base+vec(1,0,1),base+vec(0,0,1),
                vec(0,-1,0)
            ));
            visibleFacesCache[x*10000000+y*10000+z*6+3]=faces;
        }
        if(!haveB(x+1,y,z)){//右
            point_3D base(x,y,z);
            vector<CubeFace>faces;
            faces.push_back(CubeFace(
                base+vec(1,0,0),base+vec(1,1,0),
                base+vec(1,1,1),base+vec(1,0,1),
                vec(1,0,0)
            ));
            visibleFacesCache[x*10000000+y*10000+z*6+4]=faces;
        }
        if(!haveB(x-1,y,z)){//左
            point_3D base(x,y,z);
            vector<CubeFace>faces;
            faces.push_back(CubeFace(
                base+vec(0,0,0),base+vec(0,0,1),
                base+vec(0,1,1),base+vec(0,1,0),
                vec(-1,0,0)
            ));
            visibleFacesCache[x*10000000+y*10000+z*6+5]=faces;
        }
    }
    
    needUpdateFaces=false;
}

void update_phy();

void drawScene(){
    //1.用双缓冲技术来解决闪烁问题
    //2.动态刷新，无更新就不刷新，提高静止帧率("我的世界lunar端优化")
    update_phy();
    
    point_3D cam_pos;
    double cam_yaw,cam_pitch;
    vec cam_f,cam_r,cam_u;
    {
        lock_guard<recursive_mutex>lock(cam_mtx);
        cam_pos=myself.position;
        cam_yaw=myself.yaw;
        cam_pitch=myself.pitch;
        cam_f=myself.front;
        cam_r=myself.right;
        cam_u=myself.up;
    }
    camera tmp_cam=myself;
    tmp_cam.position=cam_pos;
    tmp_cam.yaw=cam_yaw;
    tmp_cam.pitch=cam_pitch;
    tmp_cam.front=cam_f;
    tmp_cam.right=cam_r;
    tmp_cam.up=cam_u;
    
    print_q.clear();
    update_map();

    //只在需要时更新可见面
    if(needUpdateFaces){
        updateAllVisibleFaces();
    }
    
    //set blocks
    point_3D light_position=tmp_cam.position;
    vector<CubeFace>visible_faces;
    vector<double>Lt;
    
    //从缓存收集可见面并计算深度
    for(auto cacheEntry:visibleFacesCache){
        for(auto face:cacheEntry.second){
            vec camera_to_face(tmp_cam.position,face.point_cube[0]);
            if(dot_product(face.normal,camera_to_face)<0){
                point_3D face_center(
                    (face.point_cube[0].x+face.point_cube[1].x+face.point_cube[2].x+face.point_cube[3].x)/4.0,
                    (face.point_cube[0].y+face.point_cube[1].y+face.point_cube[2].y+face.point_cube[3].y)/4.0,
                    (face.point_cube[0].z+face.point_cube[1].z+face.point_cube[2].z+face.point_cube[3].z)/4.0
                );
                vec depth_vec(tmp_cam.position,face_center);
                face.depth=depth_vec.length();
                visible_faces.push_back(face);
                //光强
                vec light_dir(face_center,light_position);
                light_dir.normalize();
                double light_intensity=dot_product(face.normal,light_dir);
                light_intensity=max(0.01,min(1.0,(light_intensity+1.0)/2.0));
                Lt.push_back(light_intensity);
                //cout<<light_intensity<<endl;
            }
        }
    }
    sort(visible_faces.begin(),visible_faces.end(),cmp_FaceDep);
    
    cleardevice();//清除
    setbkcolor(0xFACE87);
    setlinecolor(0x00FF00);
    setfillcolor(0);
    clearrectangle(-640,-360,640,360);
    
    vector<Edge>bian_ground;//绘制地面
    
    int face_index=0;
    for(auto face:visible_faces){//绘制正方体
        point_2D screen_points[4];
        bool all_visible=true;
        for(int i=0;i<4;i++){
            screen_points[i]=perspectiveProjection(face.point_cube[i],tmp_cam);
            if(screen_points[i].x==0&&screen_points[i].y==0){
                all_visible=false;
                break;
            }
        }
        
        if(all_visible){//光照设置入口
            POINT pts[4];
            for(int i=0;i<4;i++){
                pts[i].x=(long)screen_points[i].x;
                pts[i].y=(long)screen_points[i].y;
            }

            int base_r=(int)((face.normal.x+1)*100);
            int base_g=(int)((face.normal.y+1)*150);
            int base_b=(int)((face.normal.z+1)*200);
            
            double depth_factor=max(0.3,1.0-face.depth/100.0);
            
            int color=RGB(
                (int)(base_r*depth_factor),
                (int)(base_g*depth_factor),
                (int)(base_b*depth_factor)
            );

            setlinecolor(RGB(
                (int)(base_r*depth_factor*0.8),
                (int)(base_g*depth_factor*0.8),
                (int)(base_b*depth_factor*0.8)
            ));
            setfillcolor(color);

            solidpolygon(pts,4);
        }
        face_index++;
    }

    
    setlinecolor(0x000000);
    // 保留线段绘制，之后在绘制选中的方块时可能会用到
    for(const auto& edge:bian){//再绘制正方体
        const point_2D& p1=perspectiveProjection(edge.from,myself);
        const point_2D& p2=perspectiveProjection(edge.to,myself);
        if(p1.x!=0&&p1.y!=0&&p2.x!=0&&p2.y!=0){
            line(p1.x,p1.y,p2.x,p2.y); 
        }
    }

    setlinecolor(0x00FF00);
    line(5,0,-5,0);//准星
    line(0,5,0,-5);

    string output_string1="FPS:"+to_string(fps_out);//+"\naccuracy:"+to_string(double(click_valuble)/double(click))
    char output[100];
    for(int i=0;i<output_string1.size();i++){
        output[i]=output_string1[i];
    }
    output[output_string1.size()]='\0';

    outtextxy(300,-280,output);

    click_valuble=min(click_valuble,click_all);

    string output_string2="Destroyed blocks:"+to_string(click_valuble);//+"\naccuracy:"+to_string(double(click_valuble)/double(click))
    
    //cout<<double(click_valuble)<<" "<<double(click_all)<<endl;

    char output1[100];
    for(int i=0;i<output_string2.size();i++){
        output1[i]=output_string2[i];
    }
    output1[output_string2.size()]='\0';

    outtextxy(200,-300,output1);

    FlushBatchDraw();

    return;
}
void read_map(string name){//我们规定，默认输入为n m h为长，宽，高（注意，这里的n,m,h均是坐标最大值，总数还要+1），然后按h读入每层二维地图对应为[n][h][m]
    fstream in(name,fstream::in);
    if(!in.is_open()){//没有成功打开
        in.close();
        fstream out("map.txt",fstream::out);//那就创建默认文件
        fstream initread("map_init.txt",fstream::in);//那就创建默认文件
        int n,m,h;
        initread>>n>>m>>h;
        out<<n<<" "<<m<<" "<<h<<endl;
        int mapi;
        for(int k=0;k<=h;++k){
            for(int i=0;i<=n;i++){
                for(int j=0;j<=m;j++){
                    initread>>mapi;
                    out<<mapi<<" ";
                }
                out<<endl;
            }
            out<<endl;
        }
        out<<endl;
        out.close();
        initread.close();
        in.open("map.txt",fstream::in);
    }
    int n,m,h;
    in>>n>>m>>h;
    for(int k=0;k<=h;++k){
        for(int i=0;i<=n;++i){
            for(int j=0;j<=m;++j){
                int wi=0;
                in>>wi;
                Map[{i,k,j}]=wi;
                if(wi!=0)blocks.push_back(block(i,k,j,1));
            }
        }
    }
    in.close();
    needUpdateFaces=true;//初始需要更新面
    return;
}
void write_map(string name){//按照上述规定写地图，注意，只记录从0到max
    fstream out(name,fstream::out);
    int n=-1,m=-1,h=-1;
    for(auto B:Map){
        if(B.second){
            n=max(int(B.first.x),n);
            h=max(int(B.first.y),h);
            m=max(int(B.first.z),m);
        }
    }
    out<<n<<" "<<m<<" "<<h<<endl;
    for(int k=0;k<=h;++k){
        for(int i=0;i<=n;++i){
            for(int j=0;j<=m;++j){
                out<<Map[{i,k,j}]<<" ";
            }
            out<<endl;
        }
        out<<endl;
    }
    
    return;
}
void init_map(string name="map.txt"){
    
    read_map(name);
    //默认ground生成
    for(double i=0;i<15;i++){
        for(double j=0;j<=15;j++){
            blocks.push_back(block(j,0,i,1));
            Map[{i,0,j}]=1;
        }
    }

    needUpdateFaces=true;
    return;
}
void play(){
    Beep(532,75);
}
void play1(){
    Beep(1600,75);
}

void play2(){
    Beep(900,75);
}

bool is_jumping=false;

int land(){
    lock_guard<recursive_mutex>lock(cam_mtx);
    int x=floor(myself.position.x);
    int y=floor(myself.position.y);
    int z=floor(myself.position.z);
    for(int dy=0;dy>=-100;dy--){
        int cy=y+dy;
        if(cy<-10)break;
        if(Map[{x,cy,z}])return cy;

    }
    return -10;
}

void jump(){
    lock_guard<recursive_mutex>lock(cam_mtx);
    if(in_air)return;
    int ground=land();
    if(myself.position.y>ground+2.1)return;
    
    in_air=1;
    space=1;
    vy=0.25;
    onGround=0;

    return;
}

void update_phy(){
    static clock_t lst=clock();
    clock_t now=clock();
    double dt=double(now-lst)/CLOCKS_PER_SEC;
    if(dt<0.001)return;
    lock_guard<recursive_mutex>lock(cam_mtx);
    //cout<<"Y:"<<myself.position.y<<" InAir:"<<in_air<<" OnGround:"<<onGround<<" vy:"<<vy<<endl;
    
    if(!onGround){
        vy-=G*dt*30;
        myself.position.y+=vy*dt*30;
        int gnd=land();
        if(myself.position.y<=gnd+2){
            myself.position.y=gnd+2;
            onGround=1;
            in_air=0;
            space=0;
            vy=0;
            //cout<<" "<<myself.position.y<<endl;
        }
    }
    else if(!in_air){//实时监测
        //cout<<"BUG";
        int g=land();
        double tgt=g+2;
        if(fabs(myself.position.y-tgt)>0.01){
            myself.position.y+=(tgt-myself.position.y)*0.05;
            flag=1;
        }
    }
    lst=now;
}

void keepdrawScene(){
    while(1){
        drawScene();
    }
    return;
}
void down(){
    int y=land()+2;
    //cout<<y<<endl;
    //cout<<y;
    for(int i=1;i<=3;++i)Sleep(10);
    for(;myself.position.y>y;){
        while(DRAW){
            Sleep(1);
        }
        DRAW=true;
        flag=true;
        Sleep(10);
        myself.position.y-=0.1;
    }
    double delta=(myself.position.y-y)*0.1;
    for(double i=(myself.position.y-y);i>0;i-=0.1){
        while(DRAW){
            Sleep(1);
        }
        DRAW=true;
        flag=true;
        Sleep(10);
        myself.position.y-=delta;
    }
    is_down=false;
}
bool can_move(point_3D p){
    lock_guard<recursive_mutex>lock(cam_mtx);
    int x=floor(p.x);
    int y=floor(p.y);
    int z=floor(p.z);
    for(int dy=0;dy<=1;dy++){
        int cy=y+dy;
        if(cy>10)continue;
        if(Map[{x,cy,z}])return false;
    }
    return true;
}
void place_block(){
    //怎么判断..?
    //1.找出所有可渲染面，参照drawsence里面剔除面的方法
    //2.以x面为例，直接解出视线与当前面交点坐标，如果坐标在面内，那么就是准星正中心对准的那个面。
    //3.算出交点与摄像机距离(直接用vec的length)
    //4.找出距离最短的面
    //5.对应位置是否可以防止(地图检查),放置方块
    //一旦方块变多，性能就会降低！
    //1.不妨从射线开始走，先碰见那个方块，哪个方块就是选中方块
    //这样时间复杂度不再是O(方块总数)，而是O(视线长度/步长)
    //但是考虑到还会有不精确的问题（我不想再左右横跳了）所以这里用DDA优化

    point_3D start=myself.position;
    vec fwd=myself.front*(-1);
    
    int sx,sy,sz;
    double mx,my,mz;
    double dx,dy,dz;
    
    int nx=floor(start.x);
    int ny=floor(start.y);
    int nz=floor(start.z);
    
    sx=initsetfwd(fwd.x);
    sy=initsetfwd(fwd.y);
    sz=initsetfwd(fwd.z);
    
    //分别设置x,y,z的初始值和步长
    set_(sx,start.x,fwd.x,mx,dx,nx);
    set_(sy,start.y,fwd.y,my,dy,ny);
    set_(sz,start.z,fwd.z,mz,dz,nz);
    
    int prex=nx;
    int prey=ny;
    int prez=nz;
    
    for(int i=0;i<20;i++){
        if(Map[{nx,ny,nz}]!=0){
            if((prex!=nx||prey!=ny||prez!=nz)&&Map[{prex,prey,prez}]==0){
                int camx=floor(myself.position.x);
                int camy=floor(myself.position.y);
                int camz=floor(myself.position.z);
                
                if(!((prex==camx&&prey==camy&&prez==camz)||(prex==camx&&prey==(camy-1)&&prez==camz))){
                    Map[{prex,prey,prez}]=1;
                    blocks.push_back(block(prex,prey,prez,1));
                    needUpdateFaces=true;//标记需要更新面
                    return;
                }
            }
            
            int px=nx,py=ny,pz=nz;
            if(mx<=my&&mx<=mz){
                px+=setfwd(sx);
            }
            else if(my<=mx&&my<=mz){
                py+=setfwd(sy);
            }
            else{
                pz+=setfwd(sz);
            }
            
            if(Map[{px,py,pz}]==0){
                int camx=floor(myself.position.x);
                int camy=floor(myself.position.y);
                int camz=floor(myself.position.z);
                
                if(!((px==camx&&py==camy&&pz==camz)||(px==camx&&py==(camy-1)&&pz==camz))){
                    Map[{px,py,pz}]=1;
                    blocks.push_back(block(px,py,pz,1));
                    needUpdateFaces=true;//标记需要更新面
                }
            }
            return;
        }
        
        prex=nx;
        prey=ny;
        prez=nz;
        
        if(mx<=my&&mx<=mz){
            nx+=sx;
            mx+=dx;
        }
        else if(my<=mx&&my<=mz){
            ny+=sy;
            my+=dy;
        }
        else{
            nz+=sz;
            mz+=dz;
        }
    }
    return;
}

void init_MAP(){
    
    
    fstream init_map("map_init.txt",ios::in);
    if(!init_map.is_open()){
        fstream COUT("map_init.txt",ios::out);
        COUT<<"14 15 5\n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n";
        COUT<<"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 \n\n";

        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 1 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n\n";

        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 1 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n\n";

        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 1 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n\n";

        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n\n";

        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 1 0 1 0 1 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n";
        COUT<<"0 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n";
        COUT<<"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n\n";
        COUT.close();
    }
    init_map.close();

    return;

}
void gotoxy(int x, int y) {                 //移动光标
    COORD pos = {x,y};
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleCursorPosition(hOut, pos);
}
void init_rand_map(){
    int Time=time(NULL);
    srand(Time);
    RandMap M(15,15,rand());
    for(int i=0;i<15;++i){
        for(int j=0;j<15;++j){
            for(int k=0;k<M.mp[i][j];k++){
                blocks.push_back({i,k,j,1});
                Map[{i,k,j}]=1;
            }
        }
    }
    needUpdateFaces=true;
    return;
}
void ShowCursor(bool visible) { //显示或隐藏光标
    CONSOLE_CURSOR_INFO cursor_info = {20, visible};
    //CONSOLE_CURSOR_INFO结构体包含控制台光标信息，成员分别表示光标百分比厚度和是否可见
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursor_info);
 //SetConsoleCursorInfo设定控制台窗口的光标大小和是否可见
}
int main(){
    
    fstream log("game.log",ios::out);
    cout<<" /$$      /$$                                             /$$       /$$"<<endl;
    cout<<"| $$$    /$$$                                            | $$      | $$"<<endl;
    cout<<"| $$$$  /$$$$ /$$   /$$ /$$  /$$  /$$  /$$$$$$   /$$$$$$ | $$  /$$$$$$$"<<endl;
    cout<<"| $$ $$/$$ $$| $$  | $$| $$ | $$ | $$ /$$__  $$ /$$__  $$| $$ /$$__  $$"<<endl;
    cout<<"| $$  $$$| $$| $$  | $$| $$ | $$ | $$| $$  \\ $$| $$  \\__/| $$| $$  | $$"<<endl;
    cout<<"| $$\\  $ | $$| $$  | $$| $$ | $$ | $$| $$  | $$| $$      | $$| $$  | $$"<<endl;
    cout<<"| $$ \\/  | $$|  $$$$$$$|  $$$$$/$$$$/|  $$$$$$/| $$      | $$|  $$$$$$$"<<endl;
    cout<<"|__/     |__/ \\____  $$ \\_____/\\___/  \\______/ |__/      |__/ \\_______/"<<endl;
    cout<<"            /$$  | $$                                                "<<endl;
    cout<<"            |  $$$$$$/                                                "<<endl;
    cout<<"            \\______/                                                 "<<endl;
    cout<<endl;
    cout<<"Use 'W' to move forward\n";
    cout<<"Use 'A' to move to your left\n";
    cout<<"Use 'S' to move back\n";
    cout<<"Use 'D' to move to your right\n";
    cout<<"Click left to destroy a block\n";
    cout<<"Click right to place a block\n";
    cout<<"\nThis is a test version, if you encounter any bugs, report them with the game.log, thanks!";
    init_MAP();

    //cout<<"\nSelect a map:\n1.new_map\n";
    //1.创建地图2.打开存档3.退出
    int M=3;
    char key_input;
    bool select[10]={0,0,0,0,0};
    cout<<"\n1.Create a map\n";
    cout<<"2.Select a map\n";
    cout<<"3.exit        \n";
    while(true){
        gotoxy(0,20);
        if(select[1]){
            cout<<"$";
        }
        cout<<"1.Create a map  \n";
        if(select[2]){
            cout<<"$";
        }
        cout<<"2.Select a map  \n";
        if(select[3]){
            cout<<"$";
        }
        cout<<"3.exit          \n";
        if(_kbhit()){
            key_input=_getch();
            if(key_input>=48&&key_input<=51){
                select[1]=false;
                select[2]=false;
                select[3]=false;
                select[key_input-48]=true;
            }
            else if(key_input==13){
                if(select[1])M=1;
                else if(select[2])M=2;
                else if(select[3])M=3;
                break;
            }
        }
    }
    if(M==3)return 0;
    gotoxy(0,20);
    vector<string>names;
    namespace fs=filesystem;
    filesystem::path D=filesystem::current_path();
    //cout<<D.string()<<endl;
    for(const auto& N:fs::directory_iterator(D)){
        //cout<<N.filename().string()<<endl;
        if(N.is_regular_file()){
            if(N.path().extension().string()==".txt"&&N.path().filename().string()!="map_init.txt"){
                names.push_back(N.path().filename().string());
                //cout<<"OK";
            }
        }
    }
    for(int i=0;i<names.size();++i){
        cout<<i+1<<"."<<names[i];
        for(int j=0;j<10;++j)cout<<" ";
        cout<<endl;
    }
    string name;
    //cin>>M;
    log<<name<<endl;
    if(M==1){
        cout<<"Name of the map?(It should be ended with extension "".txt"")\n";
        for(int j=0;j<10;j++){
            for(int i=0;i<10;++i){
                cout<<" ";
            }
            cout<<endl;
        }
        cin>>name;
        int mode;
        cout<<"1.Default_map\n2.rand_map\n";
        cin>>mode;
        log<<mode<<endl;
        if(mode==1)
            init_map(name);
        else if(mode==2){
            init_rand_map();
        }
    }
    else {
        memset(select,0,sizeof(select));
        while(true){
            gotoxy(0,20);
            for(int i=0;i<names.size();++i){
                if(select[i+1])cout<<"$";
                cout<<i+1<<"."<<names[i]<<"        \n";
            }
            if(_kbhit()){
                key_input=_getch();
                if(key_input>=48&&key_input<=59){
                    memset(select,0,sizeof(select));
                    select[key_input-48]=true;
                }
                else if(key_input==13){
                    for(int i=0;i<name.size();i++){
                        if(select[i]){
                            M=i+1;
                            break;
                        }
                    }
                    break;
                }
            }
        }
        init_map(names[M-1]);
        log<<names[M-1]<<endl;
    }
    system("cls");
    cout<<" /$$   /$$                                     /$$$$$$$$                  /$$\n";
    cout<<"| $$  | $$                                    | $$_____/                 | $$\n";
    cout<<"| $$  | $$  /$$$$$$  /$$    /$$ /$$$$$$       | $$    /$$   /$$ /$$$$$$$ | $$\n";
    cout<<"| $$$$$$$$ |____  $$|  $$  /$$//$$__  $$      | $$$$$| $$  | $$| $$__  $$| $$\n";
    cout<<"| $$__  $$  /$$$$$$$ \\  $$/$$/| $$$$$$$$      | $$__/| $$  | $$| $$  \\ $$|__/\n";
    cout<<"| $$  | $$ /$$__  $$  \\  $$$/ | $$_____/      | $$   | $$  | $$| $$  | $$    \n";
    cout<<"| $$  | $$|  $$$$$$$   \\  $/  |  $$$$$$$      | $$   |  $$$$$$/| $$  | $$ /$$\n";
    cout<<"|__/  |__/ \\_______/    \\_/    \\_______/      |__/    \\______/ |__/  |__/|__/\n";

    system("title Myblock");
    //ShowCursor(false);
    initgraph(1280,720,EX_SHOWCONSOLE);
    setorigin(640,360);
    BeginBatchDraw();
    
    HWND h=GetHWnd();
    SetWindowText(h,"Myblock");

    myself=camera(point_3D(3,10,3),45,-15);

    MultiKeyDetector detector;
    POINT lastMousePos;
    GetCursorPos(&lastMousePos);
    int fps_cnt=0;
    time_t lasttime,now_time;
    time(&lasttime);
    bool click=true;

    //thread draw(keepdrawScene);
    //draw.detach();
    int cnt_y_tick=0;
    
    static auto last_tick=GetTickCount();
    while(true){
        ShowCursor(false);
        cnt_y_tick++;
        log<<"y:"<<myself.position.y<<" onGround"<<onGround<<endl;;
        if(cnt_y_tick==10){
            cnt_y_tick=0;
        }
        if(myself.position.y<0)myself.position.y=15;
        //if(onGround)myself.position.y=round(myself.position.y);//坐标格式化1.shift更自然 2.跳跃落地后坐标为整数，防止再出BUG
        //cout<<myself.position.x<<" "<<myself.position.y<<" "<<myself.position.z<<" "<<onGround<<" "<<is_down<<" "<<in_air<<endl;
        //cout<<"OK"<<endl;
        detector.updateKeyStates();
        {
            lock_guard<recursive_mutex>lock(cam_mtx);
            double last_y=myself.position.y;
            if(detector.isKeyPressed('W')){
                log<<"W"<<endl;
                myself.moveUp(-myself.moveSpeed);
                point_3D pos=myself.position;
                myself.moveUp(myself.moveSpeed);
                pos.x=pos.x+myself.front.x*(-myself.moveSpeed);
                pos.z=pos.z+myself.front.z*(-myself.moveSpeed);
                if(can_move(pos)){
                    myself.moveForward(-myself.moveSpeed);
                    flag=1;
                }
                else {
                    //cout<<"OK";
                }
            }
            if(detector.isKeyPressed('S')){
                log<<"S"<<endl;
                myself.moveUp(-myself.moveSpeed);
                point_3D pos=myself.position;
                myself.moveUp(myself.moveSpeed);
                pos.x=pos.x+myself.front.x*(myself.moveSpeed);
                pos.z=pos.z+myself.front.z*(myself.moveSpeed);
                if(can_move(pos)){
                    myself.moveForward(myself.moveSpeed);
                    flag=1;
                }
            }
            if(detector.isKeyPressed('A')){
                log<<"A"<<endl;
                myself.moveUp(-myself.moveSpeed);//这里可以解决两行高走不过去的BUG
                point_3D pos=myself.position;
                myself.moveUp(myself.moveSpeed);//关于原理，减少了y坐标防止误判
                pos=pos+myself.right;
                if(can_move(pos)){
                    myself.moveRight(myself.moveSpeed);
                    flag=1;
                }
            }
            if(detector.isKeyPressed('D')){
                log<<"D"<<endl;
                myself.moveUp(-myself.moveSpeed);
                point_3D pos=myself.position;
                myself.moveUp(myself.moveSpeed);
                pos=pos+myself.right*(-1);
                if(can_move(pos)){
                    myself.moveRight(-myself.moveSpeed);
                    flag=1;
                }
            }
            myself.position.y=last_y;
        }
        log<<"Here1 y:"<<myself.position.y<<endl;

        {
            lock_guard<recursive_mutex>lock(cam_mtx);
            if(detector.isKeyPressed(VK_SPACE)){
                log<<"SPACE_TAPPED"<<endl;
                log<<"now is:"<<last_space<<" "<<onGround<<endl;
                if((!last_space)&&onGround){
                    last_space=true;
                    onGround=false;
                    vy=0.2;
                    log<<"JUMPED"<<endl;
                }
            }
            else {
                log<<"come to false\n";
                last_space=false;
            }
            if(detector.isKeyPressed(VK_SHIFT)){
                myself.moveUp(-myself.moveSpeed);
                shift=true;
                flag=1;
            }
        }
        

        if(detector.isKeyPressed(VK_LBUTTON)){//左键部分
            log<<"L-click\n";
            if(!bclick){
                bclick=true;
                if(!click){//音效
                    thread first(play);
                    //system("C:\\Users\\yaodehao123\\Desktop\\Codes\\3D_test\\Usp.mp3");
                    //system("start mp3tag /play ""C:\\Users\\yaodehao123\\Desktop\\Codes\\3D_test\\Usp.mp3""");
                    click=true;
                    click_all++;
                    //cout<<"OK";
                    first.detach();
                }
                int count=Del();
                //add_block(count);//这里以后区分动态block和静态block
                click_valuble+=count;
                if(count){
                    thread first(play1);
                    first.detach();
                }
                flag=true;
            }

        }
        else{
            click=false;
            bclick=false;
        }

        if(detector.isKeyPressed(VK_RBUTTON)){//右键部分
            log<<"r-click\n";
            if(!rclick){
                rclick=true;
                place_block();
                thread Place(play2);
                Place.detach();
                flag=true;
            }
        }
        else{
            rclick=false;
        }
        {
            lock_guard<recursive_mutex>lock(cam_mtx);
            
            POINT currentMousePos;
            GetCursorPos(&currentMousePos);
            double xOffset=currentMousePos.x-lastMousePos.x;
            double yOffset=currentMousePos.y-lastMousePos.y;
            if(abs(xOffset)>1e-9||abs(yOffset)>1e-9)flag=true;
            myself.processMouseMovement(xOffset,yOffset);
            
            SetCursorPos(640,360);
            //HCURSOR hcur = LoadCursor(NULL, IDC_CROSS);           // 加载系统预置的鼠标样式
            //SetClassLongPtr(hwnd, GCLP_HCURSOR, (long)hcur);  // 设置窗口类的鼠标样式
            GetCursorPos(&lastMousePos);
        }
        log<<"Here y:"<<myself.position.y<<endl;
        {//重力更新
            lock_guard<recursive_mutex>lock(cam_mtx);
            if(Map[{(int)myself.position.x,floor(myself.position.y-2),int(myself.position.z)}]==0){
                flag=true;
                onGround=false;
            }
            if(!onGround){
                int T=fps_out;
                myself.position.y+=vy;
                vy-=0.001/double(T);
                if(Map[{(int)myself.position.x,ceil(myself.position.y-2),int(myself.position.z)}]!=0){
                    log<<"Jump in"<<endl;
                    onGround=true;
                    myself.position.y=ceil(myself.position.y);
                    vy=0;
                }
                flag=true;
            }
            
        }

        //只在需要时渲染
        if(flag){
            DRAW=true;
            drawScene();
            DRAW=false;
            flag=false;
        }
        if(shift==true){//调整shift，令其只能在渲染的时候向下移动
            shift=false;
            myself.moveUp(myself.moveSpeed);
        }
        if(detector.isKeyPressed(VK_ESCAPE)){
            log<<"exit\n";
            break;
        }

        fps_cnt++;
        
        time(&now_time);
        if(now_time!=lasttime){//每一秒
            log<<"FPS:"<<fps_cnt<<endl;
            myself.moveSpeed=0.1/fps_cnt*60;
            //cout<<"FPS:"<<fps_cnt<<endl;
            fps_out=fps_cnt;
            fps_cnt=0;
            lasttime=now_time;
        }
        //cout<<myself.position.x<<" "<<myself.position.y<<" "<<myself.position.z<<" "<<endl;
        
        auto now_tick=GetTickCount();
        int T=now_tick-last_tick;
        log<<"Tick:"<<T<<endl;
        if(T<16&&T>=0){
            Sleep(16-T);
        }
        last_tick=now_tick;
    }
    if(M==1){
        write_map(name);
        log<<"Saved map "<<name<<endl;
    }
    else {
        write_map(names[M-2]);
        log<<"Saved map "<<name[M-2]<<endl;
    }
    //draw.~thread();
    ShowCursor(TRUE);
    EndBatchDraw();
    closegraph();
    log<<"Programe ended"<<endl;
    log.close();
    return 0;
}