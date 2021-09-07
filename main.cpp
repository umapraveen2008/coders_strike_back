#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace std;

#define PI 3.14159265

/**
 * Auto-generated code below aims at helping you parse
 * the standard input according to the problem statement.
 **/

struct Vector2
{
    float x;
    float y;
    Vector2(float _x, float _y) : x(_x), y(_y) {}
    Vector2() : x(0.f), y(0.f) {}

    Vector2 normalized() const
    {
        const float norm = sqrt(x*x+y*y);
        return Vector2(x/norm, y/norm);
    }
};

bool operator==(const Vector2& a, const Vector2& b)
{
    return a.x == b.x && a.y == b.y;
}
bool operator!=(const Vector2& a, const Vector2& b)
{
    return a.x != b.x || a.y != b.y;
}
Vector2 operator-(const Vector2& a, const Vector2& b)
{
    return Vector2(a.x-b.x, a.y-b.y);
}
Vector2 operator*(float k, const Vector2& a)
{
    return Vector2(k*a.x, k*a.y);
}


class PlaneManager{
public:
    PlaneManager(): _isBoostAvailable(true), _allCheckpointsFound(false), _largestDistance(0.0){}

    bool IsBoostAvailable(){
        return _isBoostAvailable;
    }

    void AddNextCheckPoint(int x, int y,int distance){
        if(_allCheckpointsFound)
            return;

        const Vector2 newCheckPoint(x,y);

        if(CheckPoints.empty()){
            CheckPoints.push_back(newCheckPoint);
        }
        else if(CheckPoints.back() != newCheckPoint){
            CheckPoints.push_back(newCheckPoint);
            if(CheckPoints.front() == newCheckPoint)
            {
                _allCheckpointsFound = true;
            }
        }

        if(_largestDistance < distance){
            _largestDistance = distance;
        }
    }

    bool UseBoost(float distance)
    {
        if(_isBoostAvailable && _allCheckpointsFound && distance + errorDistance > _largestDistance){
            _isBoostAvailable = false;
            return true;
        }
        return false;
    }

private:
    const float errorDistance = 2000;
    bool _isBoostAvailable;
    bool _allCheckpointsFound;
    float _largestDistance;
    vector<Vector2> CheckPoints;
};

inline
    Vector2 rotate( const Vector2& v, float angle ) {
    float radian = angle * PI / 180;
    double sinAngle = sin(radian);
    double cosAngle = cos(radian);

    return Vector2( v.x * cosAngle - v.y * sinAngle, v.y * cosAngle + v.x * sinAngle );
}



int main()
{
    const int mAngleToSteer = 1;
    const int mDecelerationAngle = 90;
    const float mDecelerationRadius = 600 * 4;

    PlaneManager planeManager;

    
    // game loop
    while (1) {
        int x;
        int y;
        int nextCheckpointX; // x position of the next check point
        int nextCheckpointY; // y position of the next check point
        int nextCheckpointDist; // distance to the next checkpoint
        int nextCheckpointAngle; // angle between your pod orientation and the direction of the next checkpoint
        cin >> x >> y >> nextCheckpointX >> nextCheckpointY >> nextCheckpointDist >> nextCheckpointAngle; cin.ignore();
        int opponentX;
        int opponentY;
        cin >> opponentX >> opponentY; cin.ignore();

        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;
        planeManager.AddNextCheckPoint(nextCheckpointX,nextCheckpointY,nextCheckpointDist);

        // You have to output the target position
        // followed by the power (0 <= thrust <= 100)
        // i.e.: "x y thrust"
        int thrust = 100;
        bool useBoost = false;

        if(nextCheckpointAngle <= -mAngleToSteer || nextCheckpointAngle >= mAngleToSteer)
        {
              Vector2 targetDirection(nextCheckpointX-x,nextCheckpointY-y);
            targetDirection= targetDirection.normalized();

            Vector2 currentDirection = rotate(targetDirection,-nextCheckpointAngle);
            currentDirection=currentDirection.normalized();

            Vector2 steeringDirection = 100 * (targetDirection - currentDirection).normalized();

            nextCheckpointX += steeringDirection.x;
            nextCheckpointY += steeringDirection.y;

            if(nextCheckpointAngle <= -mDecelerationAngle || nextCheckpointAngle >= mDecelerationAngle)
            {
                thrust = 0;
            }
            else if(nextCheckpointDist < mDecelerationRadius){
                thrust *= (mDecelerationAngle - abs(nextCheckpointAngle))/(float)mDecelerationAngle;
            }
        }
        else{
            
            if(planeManager.UseBoost(nextCheckpointDist))
            {
                useBoost = true;
            }
            else if(nextCheckpointDist < mDecelerationRadius)
            {
                thrust *= nextCheckpointDist / mDecelerationRadius;
            }
        }

        cout << nextCheckpointX << " " << nextCheckpointY << " " ;
        
         if(useBoost){
            cout << "BOOST";
        }
        else{
            cout << thrust;
        }
        cout << endl;
    }
}