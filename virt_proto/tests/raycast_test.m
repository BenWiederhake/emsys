function raycast_test()
    map = TinMap('maps/left_right.txt', 10);
    disp(TinUtils.raycast_polymorphic(map.matrix, 100, 100, -pi/2, 400));
    disp(TinUtils.raycast_polymorphic(map.matrix, 100, 100, pi/2, 400));
end