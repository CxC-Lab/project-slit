#include <fstream>
#include <nlohmann/json.hpp>
#include "Camera.hpp"
#include "Input.hpp"
#include "Player.hpp"
#include "levels/PracticeRoom.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
bool near(float a, float b, float tolerance = 0.05f) { return std::abs(a - b) < tolerance; }
const std::vector<sf::FloatRect> floorSolids{{{0.f, 520.f}, {3200.f, 80.f}}};
void step(Player& p, const InputIntent& intent, float dt = 1.f / 60.f)
{
    p.update(intent, dt, floorSolids, 3200.f);
}
void runs()
{
    using Key = sf::Keyboard::Key;
    for (const auto key : {Key::A, Key::Left, Key::D, Key::Right})
    {
        Input input;
        Player p({1600.f, 472.f});
        input.keyPressed(key, 1.f);
        step(p, input.consume());
        check(p.state() == MovementState::Walking, "single tap walks");
        input.keyReleased(key);
        step(p, input.consume());
        input.keyPressed(key, 1.1f);
        step(p, input.consume());
        check(p.state() == MovementState::Sprinting, "horizontal double tap starts run");
        check(std::abs(p.velocity().x) > 260.f && std::abs(p.velocity().x) < Movement::runMaxSpeed,
              "run accelerates rather than snapping");
        for (int i = 0; i < 60; ++i) step(p, input.consume());
        check(p.state() == MovementState::Sprinting && near(std::abs(p.velocity().x), Movement::runMaxSpeed),
              "one-frame trigger sustains run to maximum");
        input.keyReleased(key);
        step(p, input.consume());
        check(p.state() == MovementState::Idle && near(p.velocity().x, 0.f), "release stops run");
    }
    Player p;
    step(p, {{1.f, 0.f}, false, {1.f, 0.f}});
    step(p, {{-1.f, 0.f}});
    check(p.state() == MovementState::Walking && near(p.velocity().x, -260.f), "reverse clears run without residual speed");
    step(p, {{1.f, 0.f}, false, {1.f, 0.f}});
    const float takeoffSpeed = p.velocity().x;
    step(p, {{1.f, 0.f}, true});
    check(!p.grounded() && p.state() == MovementState::Jumping && p.velocity().y < 0.f &&
          near(p.velocity().x, takeoffSpeed), "sprint jump preserves takeoff speed");
    step(p, {{1.f, 0.f}, false, {1.f, 0.f}});
    check(p.state() == MovementState::AirDashing, "same input in air starts dash");
    for (int i = 0; i < 120; ++i) step(p, {{1.f, 0.f}});
    check(p.state() == MovementState::Walking && near(p.velocity().x, 260.f), "landing does not restore old run");
    Player vertical;
    step(vertical, {{0.f, -1.f}, false, {0.f, -1.f}});
    check(vertical.state() == MovementState::Idle, "vertical double tap does not run");
    for (const int rate : {30, 60, 144})
    {
        Player timed;
        step(timed, {{1.f, 0.f}, false, {1.f, 0.f}}, 1.f / rate);
        for (int i = 1; i < rate; ++i) step(timed, {{1.f, 0.f}}, 1.f / rate);
        check(near(timed.velocity().x, 440.f), "run reaches same speed across frame rates");
        check(near(timed.collisionBounds().position.x, 513.f, 1.f), "run acceleration distance across frame rates");
    }
}
void fallAndMomentum()
{
    Player sprint({100.f, 472.f});
    step(sprint, {{1.f, 0.f}, false, {1.f, 0.f}});
    for (int i = 0; i < 30; ++i) step(sprint, {{1.f, 0.f}});
    const float start = sprint.collisionBounds().position.x;
    Player walk({start, 472.f});
    step(sprint, {{1.f, 0.f}, true});
    step(walk, {{1.f, 0.f}, true});
    check(sprint.state() == MovementState::Jumping && near(sprint.velocity().x, 440.f), "full sprint takeoff momentum");
    for (int i = 0; i < 20; ++i)
    {
        step(sprint, {{1.f, 0.f}}); step(walk, {{1.f, 0.f}});
        check(near(sprint.velocity().x, 440.f), "momentum persists in air");
    }
    check(sprint.collisionBounds().position.x - walk.collisionBounds().position.x > 60.f, "sprint jump travels farther");
    step(sprint, {});
    check(near(sprint.velocity().x, 0.f), "release cancels sprint momentum");
    step(sprint, {{-1.f, 0.f}});
    check(near(sprint.velocity().x, -260.f), "reverse uses original air control");

    Player slow({500.f, -500.f});
    Player fast({500.f, -500.f});
    Player normal({500.f, -500.f});
    for (int i = 0; i < 10; ++i) { step(slow, {}); step(fast, {}); step(normal, {}); }
    check(slow.state() == MovementState::Falling, "ordinary falling state");
    for (int i = 0; i < 10; ++i)
    {
        step(slow, {{0.f, -1.f}}); step(fast, {{0.f, 1.f}}); step(normal, {});
    }
    check(slow.state() == MovementState::SlowFalling && near(slow.velocity().y, 120.f), "slow fall limits downward speed");
    check(fast.state() == MovementState::FastFalling && fast.velocity().y > normal.velocity().y, "fast fall accelerates faster than gravity");
    step(slow, {});
    check(slow.state() == MovementState::Falling && slow.velocity().y > 120.f, "release resumes gravity");
    for (int i = 0; i < 120; ++i) step(fast, {{0.f, 1.f}}, 0.05f);
    check(fast.grounded() && fast.state() == MovementState::Idle && near(fast.collisionBounds().position.y, 472.f), "fast fall floor blocking and landing");
    Player rising;
    step(rising, {{0.f, -1.f}, true});
    Player ordinary;
    step(ordinary, {{}, true});
    check(rising.state() == MovementState::Jumping && near(rising.velocity().y, ordinary.velocity().y), "up hold leaves ascent unchanged");
    step(rising, {{0.f, 1.f}}); step(ordinary, {});
    check(near(rising.velocity().y, ordinary.velocity().y), "down hold leaves ascent unchanged");
    Player dash({500.f, -500.f});
    step(dash, {{0.f, 1.f}, false, {1.f, 0.f}});
    check(dash.state() == MovementState::AirDashing && near(dash.velocity().y, 0.f), "dash wins over fall hold");
    for (int i = 0; i < 12; ++i) step(dash, {{0.f, 1.f}});
    check(dash.state() == MovementState::FastFalling, "held down applies after dash ends");
    step(dash, {{0.f, -1.f}, false, {1.f, 0.f}});
    for (int i = 0; i < 12; ++i) step(dash, {{0.f, -1.f}});
    check(dash.state() == MovementState::SlowFalling, "held up applies after dash ends");
    const std::vector<sf::FloatRect> wall{{{40.f, -1000.f}, {24.f, 1520.f}}};
    Player sliding({64.f, -500.f});
    for (int i = 0; i < 30; ++i) sliding.update({{0.f, 1.f}}, 1.f / 60.f, wall, 3200.f);
    check(sliding.state() == MovementState::WallSliding && near(sliding.velocity().y, 90.f), "wall slide wins over fast fall");
    const std::vector<sf::FloatRect> thin{{{0.f, 300.f}, {3200.f, 1.f}}};
    Player platform({500.f, -500.f});
    for (int i = 0; i < 60; ++i) platform.update({{0.f, 1.f}}, 0.05f, thin, 3200.f);
    check(platform.grounded() && near(platform.collisionBounds().position.y, 252.f), "fast fall cannot tunnel through thin platform");
}
void practiceRoom()
{
    const PracticeRoom room;
    check(room.bounds() == sf::FloatRect({0.f, -1200.f}, {3200.f, 1800.f}), "practice bounds preserved");
    check(room.spawn() == sf::Vector2f(100.f, 472.f), "practice spawn preserved");
    check(room.solids().size() == 16, "practice terrain count preserved");
    check(room.solids().front() == sf::FloatRect({0.f, 520.f}, {3200.f, 80.f}), "practice floor preserved");
    check(room.solids()[1] == sf::FloatRect({40.f, 180.f}, {24.f, 340.f}), "practice wall preserved");
    check(room.solids().back() == sf::FloatRect({128.f, -1080.f}, {140.f, 20.f}), "practice upper platform preserved");
    Player p(room.spawn());
    p.update({}, 1.f / 60.f, room.solids(), room.bounds().size.x);
    check(p.grounded() && p.collisionBounds().position == room.spawn(), "room spawn rests on floor");
    sf::View view(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    for (int i = 0; i < 300; ++i)
        Camera::follow(view, {5000.f, -5000.f}, room.bounds(), 1.f / 60.f);
    check(near(view.getCenter().x, 2800.f) && near(view.getCenter().y, -900.f), "camera consumes room bounds");
}
void cameras()
{
    const sf::FloatRect world({0.f, -1200.f}, {3200.f, 1800.f});
    sf::View view(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    for (float y : {496.f, 460.f, 420.f, 406.f, 420.f, 496.f})
        Camera::follow(view, {500.f, y}, world, 1.f / 60.f);
    check(near(view.getCenter().y, 300.f), "ordinary floor jump does not move camera");
    view.setCenter({500.f, -300.f});
    for (float y : {-200.f, -240.f, -290.f, -240.f, -200.f})
        Camera::follow(view, {500.f, y}, world, 1.f / 60.f);
    check(near(view.getCenter().y, -300.f), "ordinary elevated jump stays in dead-zone");
    Camera::follow(view, {700.f, -700.f}, world, 1.f / 60.f);
    check(view.getCenter().y < -300.f && view.getCenter().y > -580.f, "large ascent follows without snap");
    Camera::follow(view, {900.f, 400.f}, world, 1.f / 60.f);
    check(view.getCenter().y > -327.f, "large descent follows down");
    sf::View regular = view;
    sf::View accelerated = view;
    Camera::follow(regular, {900.f, 400.f}, world, 1.f / 60.f);
    Camera::follow(accelerated, {900.f, 400.f}, world, 1.f / 60.f, Camera::catchUpMultiplier);
    check(accelerated.getCenter().y > regular.getCenter().y && accelerated.getCenter().y < 280.f,
          "fast fall camera catches up without snapping");
    const float beforeRecovery = accelerated.getCenter().y;
    Camera::follow(accelerated, {900.f, 400.f}, world, 1.f / 60.f);
    check(near(accelerated.getCenter().y, beforeRecovery + (280.f - beforeRecovery) *
               (1.f - std::exp(-6.f / 60.f))), "camera returns to original follow rate");
    float reference = 0.f;
    for (const int rate : {30, 60, 144})
    {
        view.setCenter({400.f, 300.f});
        for (int i = 0; i < rate; ++i)
            Camera::follow(view, {1600.f, -800.f}, world, 1.f / rate);
        if (rate == 30) reference = view.getCenter().y;
        check(near(view.getCenter().y, reference, 0.01f), "exponential smoothing independent of frame rate");
        check(near(view.getCenter().x, 1600.f), "X direct follow preserved");
    }
    for (int i = 0; i < 300; ++i) Camera::follow(view, {-500.f, -5000.f}, world, 1.f / 60.f);
    check(near(view.getCenter().x, 400.f) && near(view.getCenter().y, -900.f), "left/top world clamps");
    for (int i = 0; i < 300; ++i) Camera::follow(view, {5000.f, 5000.f}, world, 1.f / 60.f);
    check(near(view.getCenter().x, 2800.f) && near(view.getCenter().y, 300.f), "right/bottom world clamps");
}
void climbingRoute()
{
    const std::vector<sf::FloatRect> solids{
        {{0.f, 520.f}, {3200.f, 80.f}}, {{40.f, 180.f}, {24.f, 340.f}},
        {{40.f, -1050.f}, {24.f, 1230.f}},
        {{128.f, 80.f}, {140.f, 20.f}}, {{128.f, -160.f}, {140.f, 20.f}},
        {{128.f, -400.f}, {140.f, 20.f}}, {{128.f, -640.f}, {140.f, 20.f}},
        {{128.f, -880.f}, {140.f, 20.f}}, {{128.f, -1080.f}, {140.f, 20.f}}
    };
    Player p;
    for (int i = 0; i < 20; ++i) p.update({{-1.f, 0.f}}, 1.f / 60.f, solids, 3200.f);
    check(p.touchingWall(), "climbing wall accessible from spawn");
    for (int i = 0; i < 240; ++i)
        p.update({{}, i % 10 == 0}, 1.f / 60.f, solids, 3200.f);
    check(p.collisionBounds().position.y < -1000.f, "wall seam permits repeated jumps into upper world");
    // A top-up dash and rightward movement reach the highest resting platform.
    p.update({{}, false, {0.f, -1.f}}, 0.05f, solids, 3200.f);
    for (int i = 0; i < 25; ++i) p.update({{1.f, 0.f}}, 1.f / 60.f, solids, 3200.f);
    for (int i = 0; i < 90; ++i) p.update({}, 1.f / 60.f, solids, 3200.f);
    check(p.grounded() && near(p.collisionBounds().position.y, -1128.f), "highest platform reachable and landable");
}
}
void regionData()
{
    const auto path = Region::findFile("practice_room");
    check(Region::findFile("practice_room", path.parent_path()) == path, "ancestor path search");
    Region large(Region::findFile("avatar_lake"));
    check(large.bounds() == sf::FloatRect({0.f,-4800.f},{9600.f,5400.f}), "avatar_lake twelve by twelve screens");
    check(large.solids().size() > 16, "avatar_lake geometry loaded");
    nlohmann::json data;
    std::ifstream(path) >> data;
    const auto temporary = std::filesystem::temp_directory_path() / "project_slit_region_test.json";
    for (int i = 0; i < 4; ++i) {
        auto invalid = data;
        if (i == 0) invalid.erase("spawn");
        if (i == 1) invalid["bounds"][2] = "wide";
        if (i == 2) invalid["solids"][0][2] = -1;
        if (i == 3) invalid["bounds"][2] = 100;
        { std::ofstream output(temporary); output << invalid; }
        bool rejected = false;
        try { Region bad(temporary); } catch (const std::exception&) { rejected = true; }
        std::filesystem::remove(temporary);
        check(rejected, "missing/type/geometry/camera-size errors rejected");
    }
}
void sustainedCamera(float speed)
{
    const sf::FloatRect world({0,-100000},{3200,200000});
    for(float height : {450.f,600.f}) for(int fps : {30,60,144}) {
        sf::View view(sf::FloatRect({0,0},{800,height}));
        view.setCenter({500,0});
        float y=0;
        for(int frame=0;frame<fps*5;++frame) {
            y+=speed/fps;
            Camera::follow(view,{500,y},world,1.f/fps,Camera::speedMultiplier(speed));
            check(std::abs(y-view.getCenter().y)+Movement::collisionSize.y/2 < height/2,
                  "sustained vertical movement keeps full collider inside view");
        }
    }
}
void terminalFalls()
{
    const std::vector<sf::FloatRect> solids{{{0,90000},{800,80}}};
    Player normal({100,0}), fast({100,0});
    float normalPeak=0,fastPeak=0;
    for(int i=0;i<600;++i) {
        normal.update({},1.f/60,solids,800);
        fast.update({{0,1}},1.f/60,solids,800);
        normalPeak=std::max(normalPeak,normal.velocity().y);
        fastPeak=std::max(fastPeak,fast.velocity().y);
    }
    check(near(normalPeak,Movement::fallMaxSpeed) && near(fastPeak,Movement::fastFallMaxSpeed) && normalPeak<fastPeak,
          "actual normal and fast falls reach ordered terminal speeds");
    check(normalPeak>Movement::landImpactSpeed,"normal terminal fall can trigger Landing");
}
void continuousCamera()
{
    const sf::FloatRect world({0,-100000},{3200,200000});
    float previous=Camera::speedMultiplier(-1200),previousY=0;
    for(int i=-12000;i<=12000;++i) {
        const float speed=i/10.f;
        const float multiplier=Camera::speedMultiplier(speed);
        sf::View view(sf::FloatRect({0,0},{800,450}));
        view.setCenter({500,0});
        Camera::follow(view,{500,200},world,1.f/60,multiplier);
        check(std::abs(multiplier-previous)<.001f,"speed sweep has no multiplier step");
        if(i>-12000) check(std::abs(view.getCenter().y-previousY)<.01f,"speed sweep has no camera displacement step");
        previous=multiplier; previousY=view.getCenter().y;
    }
    check(near(Camera::speedMultiplier(0),1) && Camera::speedMultiplier(-520)<=1.031f,
          "walking and single jump preserve gentle tracking");
    const float normalLag=Movement::fallMaxSpeed/(6*Camera::speedMultiplier(Movement::fallMaxSpeed));
    const float fastLag=Movement::fastFallMaxSpeed/(6*Camera::speedMultiplier(Movement::fastFallMaxSpeed));
    check(normalLag<=fastLag,"normal fall camera lag does not exceed fast fall");
}
int main()
{
    try { sustainedCamera(-700); sustainedCamera(Movement::fallMaxSpeed); terminalFalls(); continuousCamera(); regionData(); runs(); fallAndMomentum(); cameras(); climbingRoute(); practiceRoom(); std::cout << "PASS: running, vertical camera, climbing route\n"; }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
