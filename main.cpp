#include <algorithm>
#include <chrono>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>


using namespace std;

constexpr float PI = 3.14159f;

float clip(float n, float lower, float upper) {
    return std::max(lower, std::min(n, upper));
}

inline int fastrand()
{
    static unsigned int g_seed = 100;
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16)&0x7FFF;
}

inline int Random(int a, int b)
{
    return (fastrand() % (b-a)) + a;
}

constexpr int PREDICTIONTURNS = 4;
constexpr int PREDICTIONCOUNT = 6;

constexpr int maxThrust = 100;
constexpr int boostThrust = 650;
constexpr int maxRotation = 18;

constexpr int SHIELDCOOLDOWN = 4;
constexpr float checkpointRadius = 600.f;
constexpr float checkpointRadiusSqr = checkpointRadius * checkpointRadius;

constexpr float podRadius = 400.f;
constexpr float podRadiusSqr = podRadius * podRadius;

constexpr float minImpulse = 120.f;
constexpr float frictionFactor = .85f;

constexpr int timeoutFirstTurn = 500; //ms
constexpr int timeout = 75; //ms

constexpr int totalPodsInLevel = 4;
constexpr int playerPods = 2;


struct Vector2
{
    float x;
    float y;
    Vector2(float _x, float _y) : x(_x), y(_y) {}
    Vector2() : x(0.f), y(0.f) {}

    Vector2 normalized() const
    {
        const float norm = magnitued();
        return {x/norm, y/norm};
    }
    
    float magnitued() const{
        return sqrt(x * x + y * y);
    }

    float distSqr(const Vector2& b) const
    {
        return (x - b.x)*(x - b.x) + (y - b.y)*(y - b.y);
    }

    float dist(const Vector2& b) const
    {
        return sqrt(distSqr(b));
    }

    void ToInt(){
        x = (int)x;
        y = (int)y;
    }

    void Round(){
        x= round(x);
        y= round(y);
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
Vector2 operator+(const Vector2& a, const Vector2& b)
{
    return {a.x+b.x, a.y+b.y};
}
Vector2 operator+=(Vector2& a, const Vector2& b)
{
    return a = a+b;
}
Vector2 operator-(const Vector2& a, const Vector2& b)
{
    return {a.x-b.x, a.y-b.y};
}
Vector2 operator*(float k, const Vector2& a)
{
    return {k*a.x, k*a.y};
}
Vector2 operator*=(Vector2& a, float k)
{
    return a = k*a;
}
ostream& operator<<(ostream& os, const Vector2& v)
{
    os << v.x << " " << v.y;
    return os;
}


float dot(const Vector2& a, const Vector2& b)
{
    return a.x*b.x + a.y*b.y;
}

struct PodMovement
{
    int rotation = 0;
    int thrust = 0;
    bool shield = false;
    bool boost = false;
};

class PredictionTurn{
private:
    vector<PodMovement> moves = vector<PodMovement>(2);
public:
    PodMovement& operator[](size_t m)
    {
        return moves[m];
    }

    const PodMovement& operator[](size_t m) const
    {
        return moves[m];
    }
};


class Prediction
{
private:
    vector<PredictionTurn> turns = vector<PredictionTurn>(PREDICTIONTURNS);
public:
    PredictionTurn& operator[](size_t t)
    {
        return turns[t];
    }
    const PredictionTurn& operator[](size_t t) const
    {
        return turns[t];
    }
    int predictionScore = -1;

    bool operator < (const Prediction& other) const
    {
        return predictionScore < other.predictionScore;
    }

    bool operator > (const Prediction& other) const
    {
        return predictionScore > other.predictionScore;
    }
};


class Pod
{
private:
    bool isBoostAvailable;
public:
    int totalCheckPointsPassed  = 0;
    int targetCheckpointID = 0;
    int shieldCD = 0;
    int score;
    int angle = -1;
    Vector2 position;
    Vector2 velocity;
public:
   Pod():isBoostAvailable(true){}

   Pod(const Pod& pod)
   {
       totalCheckPointsPassed = pod.totalCheckPointsPassed;
       targetCheckpointID = pod.targetCheckpointID;
       shieldCD = pod.shieldCD;
       score = pod.score;
       angle = pod.angle;
       position = pod.position;
       velocity = pod.velocity;
       isBoostAvailable = pod.isBoostAvailable;
   }
   bool IsBoostAvailable() const{
       return isBoostAvailable;
   }

   bool UseBoost(){
       if(isBoostAvailable) {
           isBoostAvailable = false;
           return true;
       }
       return false;
   }

   void ManageShield(bool on)
   {
       if(on)
           shieldCD = SHIELDCOOLDOWN;
       else if(shieldCD > 0)
           --shieldCD;
   }

    float CollisionTime(Pod& otherPod) const
    {
        const Vector2 dPosition = (otherPod.position - position);
        const Vector2 dVelocity = (otherPod.velocity - velocity);

        constexpr float TOLERANCE =  0.000001f;

        const float dotVelocity = dot(dVelocity,dVelocity);

        if(dotVelocity < TOLERANCE)
            return INFINITY;

        const float dotPositionVelocity = -2.f*dot(dPosition,dVelocity);
        const float dotPosition = dot(dPosition,dPosition) - 4.f*podRadiusSqr;

        const float delta = dotPositionVelocity * dotPositionVelocity - 4.f * dotVelocity * dotPosition;
        if(delta < 0.f)
            return INFINITY;
        const float collisionTime = (dotPositionVelocity - sqrt(delta)) / (2.f * dotVelocity);
        if(collisionTime <= TOLERANCE)
            return INFINITY;

        return collisionTime;
    }

    float Mass() const
    {
        return shieldCD == SHIELDCOOLDOWN ? 10.f : 1.f;
    }

    void Rebound(Pod& otherPod)
    {
        const float m1 = Mass();
        const float m2 = otherPod.Mass();

        const Vector2 dP = otherPod.position - position;
        const float AB = position.dist(otherPod.position);

        const Vector2 u = (1.f/AB) * dP;

        const Vector2 dS = otherPod.velocity - velocity;

        const float m = (m1*m2)/(m1+m2);
        const float k = dot(dS,u);

        const float impulse = -2.f * m * k;
        const float impulseToUse = clip(impulse,-minImpulse,minImpulse);

        velocity +=  (-1.f/m1) * impulseToUse * u;
        otherPod.velocity += (1.f/m2) * impulseToUse * u;
    }

    void EvaluateScore(const Vector2& targetPosition){
        constexpr int cpFactor = 30000;
        const int distToCp =   position.dist(targetPosition);
        score = cpFactor * totalCheckPointsPassed - distToCp;
   }
};


class Level
{
private:
    vector<Vector2> checkPoints;
    int laps;
    int lapCheckpointCount;

public:
    vector<Pod> originalPods = vector<Pod>(totalPodsInLevel);
    vector<Pod> pods;

    int TotalCheckpointsRequired() const{
        return laps * lapCheckpointCount;
    }

    void ReadCheckpoints(){
        cin >> laps; cin.ignore();
        cin >> lapCheckpointCount; cin.ignore();

        checkPoints.reserve(lapCheckpointCount);

        for (int i = 0; i < lapCheckpointCount; ++i) {
            int checkpointX,checkpointY;
            cin >> checkpointX >> checkpointY; cin.ignore();
            checkPoints[i] = Vector2(checkpointX,checkpointY);
        }
    }

    Vector2 GetCheckpointPosition(const int& index)
    {
        return checkPoints[index];
    }

    void GenerateOutput(const Prediction& prediction)
    {
        constexpr int predicitionIndex = 0;
        for (int i = 0; i < playerPods; i++) {
            const Pod& pod = originalPods[i];
            const PodMovement& movement = prediction[predicitionIndex][i];

            const float angle = (pod.angle + movement.rotation) % 360;
            const float rAngle = angle * PI / 180.f;

            constexpr float imaginaryDistance = 10000.f;
            const Vector2 direction{imaginaryDistance * cos(rAngle), imaginaryDistance * sin(rAngle)};
            const Vector2 podTargetPosition = pod.position + direction;

            cout << round(podTargetPosition.x) << " " << round(podTargetPosition.y) << " ";

            if(movement.shield)
                cout << "SHIELD";
            else if(movement.boost)
                cout << "BOOST";
            else
                cout << movement.thrust;

            cout << endl;
        }
    }

    void ReadPods(){
        for (int i = 0; i < totalPodsInLevel; i++) {
            Pod& pod = originalPods[i];
            int x, y, vx, vy, angle, nextCheckPointId;
            cin >> x >> y >> vx >> vy >> angle >> nextCheckPointId; cin.ignore();
            pod.position = Vector2(x,y);
            pod.velocity = Vector2(vx,vy);

            if(pod.angle == -1)
            {
                const Vector2 direction = (GetCheckpointPosition(1) - pod.position).normalized();
                angle = acos(direction.x) * 180.f / PI;
                if(direction.y < 0)
                    angle = (360.f - angle);
            }
            pod.angle= angle;
            if(pod.targetCheckpointID != nextCheckPointId)
                pod.totalCheckPointsPassed++;

            pod.targetCheckpointID = nextCheckPointId;
        }
    }

    void ExecutePrediction(const Prediction& prediction)
    {
        for (int i = 0; i < PREDICTIONTURNS; i++) {
            ExecuteOneTurn(prediction[i]);
        }
    }

    void ResetAbilites(const Prediction& prediction) {
        for (int i = 0; i < playerPods; i++) {
            Pod& pod = originalPods[i];
            const PodMovement& movement = prediction[0][i];

            pod.ManageShield(movement.shield);

            if(pod.shieldCD == 0 && movement.boost)
                pod.UseBoost();
        }
    }



    int GetPredictionScore()
    {
        for (Pod& pod : pods ) {
            pod.EvaluateScore(checkPoints[pod.targetCheckpointID]);
        }


        //Need to change this logic if got more than 2 pods per team.
        const int playerPodWithBestScore = (pods[0].score > pods[1].score) ? 0 : 1;
        const Pod& mainPod = pods[playerPodWithBestScore];
        const Pod& otherPlayerPod = pods[1 - playerPodWithBestScore];

        const Pod& enemyPod = (pods[2].score > pods[3].score) ? pods[2] : pods[3];

        if(mainPod.totalCheckPointsPassed > TotalCheckpointsRequired())
            return INFINITY;
        if(enemyPod.totalCheckPointsPassed > TotalCheckpointsRequired())
            return -INFINITY;

        const int score = mainPod.score - enemyPod.score;

        const Vector2 enemyCheckpoint = checkPoints[enemyPod.targetCheckpointID];
        const int otherPlayerScore = -otherPlayerPod.position.dist(enemyCheckpoint);

        constexpr int bias = 2;

        return score * bias + otherPlayerScore;
    }

private:
    void ExecuteOneTurn(const PredictionTurn& turn)
    {
        PredicteRotation(turn);
        PredicteAcceleration(turn);
        PredicteUpdatedPositions();
        PredicteFriction();
        EndPrediction();
    }

    void PredicteRotation(const PredictionTurn& turn){
        for (int i = 0; i < playerPods; i++) {
            Pod& pod = pods[i];
            const PodMovement& movement = turn[i];
            pod.angle = (pod.angle + movement.rotation) % 360;
        }
    }

    void PredicteAcceleration(const PredictionTurn& turn)
    {
        for (int i = 0; i < playerPods; i++) {
            Pod& pod = pods[i];
            const PodMovement& movement = turn[i];
            pod.ManageShield(movement.shield);
            if(pod.shieldCD > 0)
                continue;

            const float rAngle = pod.angle * PI / 180.f;
            const Vector2 direction{cos(rAngle),sin(rAngle)};

            const bool useBoost = pod.IsBoostAvailable() && movement.boost;
            const int thrust = useBoost ? boostThrust : movement.thrust;

            if(useBoost)
                pod.UseBoost();

            pod.velocity += thrust * direction;
        }
    }

    void PredicteUpdatedPositions()
    {
        float t = 0.f;
        constexpr float turnEnd = 1.f;
        while (t < turnEnd)
        {
            Pod* pod1 = nullptr;
            Pod* pod2 = nullptr;
            float dt = turnEnd - t;
            for (int i = 0; i < totalPodsInLevel; i++) {
                for (int j = i+1; j < totalPodsInLevel; j++) {
                    const float collisionTime = pods[i].CollisionTime(pods[j]);
                    if((t + collisionTime < turnEnd) && (collisionTime < dt))
                    {
                        dt = collisionTime;
                        pod1 = &pods[i];
                        pod2 = &pods[j];
                    }
                }
            }

            for (Pod& pod: pods) {
                pod.position += dt * pod.velocity;
                if(pod.position.distSqr(checkPoints[pod.targetCheckpointID]) < checkpointRadiusSqr)
                {
                    pod.targetCheckpointID = (pod.targetCheckpointID + 1) % lapCheckpointCount;
                    ++pod.totalCheckPointsPassed;
                }
            }

            if(pod1 != nullptr && pod2 != nullptr)
            {
                pod1->Rebound(*pod2);
            }

            t+=dt;
        }
    }

    void PredicteFriction()
    {
        for (Pod& pod: pods) {
            pod.velocity *= frictionFactor;
        }
    }

    void EndPrediction()
    {
        for (Pod& pod: pods)
        {
            pod.velocity.ToInt();
            pod.position.Round();
        }
    }
};


class PredictionManager
{
private:
    vector<Prediction> predictions;
    Level* currentLevel;
public:
    PredictionManager() = delete;



    PredictionManager(Level* _level)
    {
        currentLevel = _level;
        InitiatePredictions();
        FirstTurnBoost();
    }

    const Prediction& GenerateOptimizedPrediction(int time)
    {
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
        auto Predicting = [&start, time]() -> bool
        {
            auto now = high_resolution_clock::now();
            auto dMilliseconds = duration_cast<milliseconds>(now - start);
            return (dMilliseconds.count() < time);
        };

        for (int i = 0; i < PREDICTIONCOUNT; i++) {
            Prediction& prediction = predictions[i];
            ShiftByOneTurn(prediction);
            ComputeScore(prediction);
        }

        while (Predicting())
        {
            for (int i = 0; i < PREDICTIONCOUNT; ++i) {
                Prediction& newPrediction = predictions[PREDICTIONCOUNT + i];
                newPrediction = predictions[i];
                Mutate(newPrediction);
                ComputeScore(newPrediction);
            }

            std::sort(predictions.begin(),predictions.end(),std::greater<Prediction>());
        }
        return predictions[0];
    }

private:

    void InitiatePredictions() {
        predictions.resize(2 * PREDICTIONCOUNT);
        for (int i = 0; i < PREDICTIONCOUNT; i++) {
            for (int j = 0; j < PREDICTIONTURNS; j++) {
                for (int k = 0; k < 2; k++) {
                    RandomMovement(predictions[i][j][k]);
                }
            }
        }
    }

    void FirstTurnBoost() {
        const float d = currentLevel->GetCheckpointPosition(0).distSqr(currentLevel->GetCheckpointPosition(1));
        constexpr float boostThreshold = 9000000.f;

        if(d < boostThreshold)
            return;

        for (int i = 0; i < playerPods; i++) {
            for (int j = 0; j < PREDICTIONCOUNT; j++) {
                predictions[j][0][i].boost = true;
            }
        }
    }


    void ShiftByOneTurn(Prediction& prediction) const
    {
        for (int i = 1; i < PREDICTIONTURNS; i++) {
            for (int j = 0; j < 2; j++)
            {
                prediction[i-1][j] = prediction[i][j];
            }
        }
        for (int i = 0; i < 2; i++) {
            PodMovement& movement = prediction[PREDICTIONTURNS-1][i];
            RandomMovement(movement);
        }
    }

    int ComputeScore(Prediction& prediction)
    {
        currentLevel->pods = vector<Pod>(currentLevel->originalPods);
        currentLevel->ExecutePrediction(prediction);
        prediction.predictionScore = currentLevel->GetPredictionScore();
        return prediction.predictionScore;
    }

    void Mutate(Prediction& prediction) const
    {
        int k = Random(0, 2 * PREDICTIONTURNS);
        PodMovement& movement= prediction[k/2][k%2];
        RandomMovement(movement, false);
    }

    void RandomMovement(PodMovement& podMovement,bool predictAll = true) const
    {
        constexpr int all = -1, rotation = 0, thrust = 1, shield =2, boost = 3;
        constexpr int         pr=5      , pt=pr+5,  ps=pt+1,  pb=ps+0;
        const int propertyToModify = Random(0,4);

        const int i = predictAll ? -1 : Random(0, pb);
        const int valueToModify = [i, predictAll]() -> int
        {
            if (predictAll)
                return true;
            if (i<=pr)
                return rotation;
            if (i<=pt)
                return thrust;
            if (i<=ps)
                return shield;
            return boost;
        }();

        auto modifyValue = [valueToModify](int parValue)
        {
            if (valueToModify == all)
                return true;
            return valueToModify == parValue;
        };


        if(modifyValue(rotation))
        {
            const int rotationValue = Random( -2 * maxRotation , 3 * maxRotation);
            if(rotationValue > 2 * maxRotation)
                podMovement.rotation = 0;
            else
                podMovement.rotation = clip(rotationValue,-maxRotation,maxRotation);
        }

        if(modifyValue(thrust))
        {
            podMovement.thrust = clip(Random(-0.5f * maxThrust,2 * maxThrust),0,maxThrust);
        }

        if(modifyValue(shield))
        {
            if(!predictAll || (Random(0,10) > 6))
                podMovement.shield = !podMovement.shield;
        }

        if(modifyValue(boost))
        {
            if(!predictAll || (Random(0,10) > 6))
                podMovement.boost = !podMovement.boost;
        }
    }
};


int main()
{
    Level level;
    level.ReadCheckpoints();

    PredictionManager predictionManager(&level);
    bool firstLoop = true;
    while (1)
    {
        level.ReadPods();

        const int time = firstLoop?timeoutFirstTurn:timeout;
        firstLoop = false;
        constexpr float timeoutSafeGuard = .95f;

        const Prediction& prediction = predictionManager.GenerateOptimizedPrediction(time * timeoutSafeGuard);
        level.GenerateOutput(prediction);
        level.ResetAbilites(prediction);
    }
}