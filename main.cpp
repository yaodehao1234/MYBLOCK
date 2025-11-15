#include<iostream>
#include<vector>
#include<cmath>
#include<graphics.h>
#include<easyx.h>
#include<map>
#include<windows.h>
#include<ctime>

#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0) 
#define Pi 3.1415926535

using namespace std;

class point_3D;
class point_2D;
class vec;

class MultiKeyDetector {
private:
    map<int, bool> keyStates;
    
public:
    MultiKeyDetector() {
        keyStates[VK_UP] = false;
        keyStates[VK_DOWN] = false;
        keyStates[VK_LEFT] = false;
        keyStates[VK_RIGHT] = false;
        keyStates['W'] = false;
        keyStates['A'] = false;
        keyStates['S'] = false;
        keyStates['D'] = false;
        keyStates['Q'] = false;
        keyStates['E'] = false;
        keyStates[VK_SPACE] = false;
        keyStates[VK_SHIFT] = false;
        keyStates[VK_CONTROL] = false;
        keyStates[VK_ESCAPE] = false;
    }
    
    void updateKeyStates() {
        for (auto& pair :keyStates) {
            pair.second=(GetAsyncKeyState(pair.first)&0x8000)!=0;
        }
    }
    
    bool isKeyPressed(int virtualKey) {
        return keyStates[virtualKey];
    }
};

class point_3D {
public:
    double x, y, z;
    point_3D(double x=0,double y=0,double z=0):x(x),y(y),z(z){}
    point_3D(const point_3D& p):x(p.x),y(p.y),z(p.z){}
};

class vec {
public:
    double x, y, z;
    vec(double x=0,double y=0,double z=0):x(x),y(y),z(z) {}
    vec(const point_3D& a,const point_3D& b):x(b.x-a.x),y(b.y-a.y),z(b.z-a.z){}
    double length(){
        return sqrt(x*x+y*y+z*z);
    }
    void normalize(){
        double len=length();
        if (len>0) {
            x/=len;
            y/=len;
            z/=len;
        }
    }
    friend vec operator * (const vec& a,double k) {
        return vec(a.x*k,a.y*k,a.z*k);
    }
    
    friend vec operator + (const vec& a,const vec& b) {
        return vec(a.x+b.x,a.y+b.y,a.z+b.z);
    }
};

point_3D operator + (const point_3D& a,const vec& b) {
    return point_3D(a.x+b.x,a.y+b.y,a.z+b.z);
}

double dot_product(const vec& a,const vec& b) {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}

vec cross_product(const vec& a,const vec& b) {
    return vec(
        a.y*b.z-a.z*b.y,
        a.z*b.x-a.x*b.z,
        a.x*b.y-a.y*b.x
    );
}

class camera{
public:
    point_3D position;
    double yaw, pitch;
    double moveSpeed;
    double mouseSensitivity;
    vec front,right,up;
    camera(const point_3D& pos=point_3D(0, 0, 0),double yaw=-90.0,double pitch=0.0) 
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
        position=position+front*distance;
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
    point_2D(double x,double y):x(x),y(y){}
    point_2D(const point_2D& p):x(p.x),y(p.y){}
};

class Edge{
public:
    int from,to;
    Edge(int from,int to):from(from),to(to){}
};

vector<point_3D>point_q;
vector<point_2D>print_q;
vector<Edge>bian;
camera myself;

// 透视投影函数
point_2D perspectiveProjection(const point_3D& point,const camera& cam){
    vec viewVector(point, cam.position);
    double x=dot_product(viewVector,cam.right);
    double y=dot_product(viewVector,cam.up);
    double z=dot_product(viewVector,cam.front);
    if(z<=0.1)return point_2D(0,0);
    double fov=90.0;
    double aspectRatio=800.0/640.0;
    double nearPlane=0.1;
    double farPlane=100.0;
    double scale=500.0/z;
    return point_2D(x*scale,y*scale);
}

void drawScene(){
    setlinecolor(0x00FF00);
    setfillcolor(0);
    clearrectangle(-400,-320,400,320);
    line(5,0,-5,0);
    line(0,5,0,-5);
    print_q.clear();
    for (const auto& point:point_q) {
        point_2D projected=perspectiveProjection(point, myself);
        print_q.push_back(projected);
    }
    for (const auto& edge:bian) {
        const point_2D& p1=print_q[edge.from];
        const point_2D& p2=print_q[edge.to];
        if (p1.x!=0&&p1.y!=0&&p2.x!=0&&p2.y!=0) {
            line(p1.x,p1.y,p2.x,p2.y);
        }
    }
}

int main() {
    initgraph(800,640,EX_SHOWCONSOLE);
    setorigin(400,320);
    
    myself=camera(point_3D(3,1.5,3),45,-15);
    
    point_q.push_back(point_3D(0,0,0));
    point_q.push_back(point_3D(2,0,0));
    point_q.push_back(point_3D(2,2,0));
    point_q.push_back(point_3D(0,2,0));
    point_q.push_back(point_3D(0,0,2));
    point_q.push_back(point_3D(2,0,2));
    point_q.push_back(point_3D(2,2,2));
    point_q.push_back(point_3D(0,2,2));
    bian.push_back(Edge(0,1));
    bian.push_back(Edge(1,2));
    bian.push_back(Edge(2,3));
    bian.push_back(Edge(3,0));
    bian.push_back(Edge(4,5));
    bian.push_back(Edge(5,6));
    bian.push_back(Edge(6,7));
    bian.push_back(Edge(7,4));
    bian.push_back(Edge(0,4));
    bian.push_back(Edge(1,5));
    bian.push_back(Edge(2,6));
    bian.push_back(Edge(3,7));

    MultiKeyDetector detector;
    
    POINT lastMousePos;
    GetCursorPos(&lastMousePos);
    
    int fps_cnt=0;
    time_t lasttime,now_time;
    time(&lasttime);
    
    while(true){
        detector.updateKeyStates();
        if(detector.isKeyPressed('W')){
            myself.moveForward(-myself.moveSpeed);
        }
        if(detector.isKeyPressed('S')){
            myself.moveForward(myself.moveSpeed);
        }
        if(detector.isKeyPressed('A')){
            myself.moveRight(myself.moveSpeed);
        }
        if(detector.isKeyPressed('D')){
            myself.moveRight(-myself.moveSpeed);
        }
        if(detector.isKeyPressed(VK_SPACE)){
            myself.moveUp(myself.moveSpeed);
        }
        if(detector.isKeyPressed(VK_SHIFT)){
            myself.moveUp(-myself.moveSpeed);
        }
        
        POINT currentMousePos;
        GetCursorPos(&currentMousePos);
        double xOffset=currentMousePos.x-lastMousePos.x;
        double yOffset=currentMousePos.y-lastMousePos.y;
        
        myself.processMouseMovement(xOffset,yOffset);
        
        SetCursorPos(400,320);
        GetCursorPos(&lastMousePos);
        drawScene();
    
        if(detector.isKeyPressed(VK_ESCAPE)) {
            break;
        }
        
        fps_cnt++;
        time(&now_time);
        if (now_time!=lasttime) {
            myself.moveSpeed=0.1/fps_cnt*60;
            cout<<"FPS:"<<fps_cnt<<endl;
            fps_cnt=0;
            lasttime=now_time;
        }
        
        Sleep(5);
    }
    
    ShowCursor(TRUE);
    closegraph();

    return 0;
}