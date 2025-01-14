local tcname = 'tests.e2.buildings'
local lunit = require('lunit')
if _VERSION >= 'Lua 5.2' then
  _ENV = module(tcname, 'seeall')
else
  module(tcname, lunit.testcase, package.seeall)
end

function setup()
    eressea.game.reset()
end

function test_castle_names()
    local r = region.create(0, 0, "plain")
    local b = building.create(r, "castle")

    assert_equal("site", b:get_typename(1))
    assert_equal("tradepost", b:get_typename(2))
    assert_equal("tradepost", b:get_typename(9))
    assert_equal("fortification", b:get_typename(10))
    assert_equal("fortification", b:get_typename(49))
    assert_equal("tower", b:get_typename(50))
    assert_equal("tower", b:get_typename(249))
    assert_equal("castle", b:get_typename(250))
    assert_equal("castle", b:get_typename(1249))
    assert_equal("fortress", b:get_typename(1250))
    assert_equal("fortress", b:get_typename(6249))
    assert_equal("citadel", b:get_typename(6250))
end

function test_build_tunnel_limited()
    -- bug 2221
    local r = region.create(0, 0, "plain")
    local b = building.create(r, "tunnel")
    local f = faction.create('human')
    local u = unit.create(f, r, 2)
    u:set_skill('building', 6, true)
    u:add_item('stone', 22)
    u:add_item('log', 10)
    u:add_item('iron', 2)
    u:add_item('money', 700)
    u.building = b
    u:add_order('MACHE 2 BURG ' .. itoa36(b.id))
    b.size = 99
    process_orders()
    assert_equal(100, b.size)
end

function test_build_castle_one_stage()
    local r = region.create(0, 0, 'plain')
    local f = faction.create('human')
    local u = unit.create(f, r, 2)

    u:add_item('stone', 4)

    u:set_skill('building', 1)
    u:add_order('MACHE BURG')

    process_orders()
    assert_equal(2, u.building.size)
    assert_equal(2, u:get_item('stone'))
end

function test_build_castle_stages()
    local r = region.create(0,0, "plain")
    local f = faction.create("human")
    local u = unit.create(f, r, 1000)
    local b = building.create(r, "castle")
    assert_equal("Burg", b.name)

    u:add_item("stone", 1000)

    u:set_skill("building", 1)
    u:clear_orders()

    u:add_order("MACHE BURG " .. itoa36(b.id))
    process_orders()
    assert_equal(10, b.size)
    
    u:set_skill("building", 3)
    u:clear_orders()
    
    u:add_order("MACHE BURG " .. itoa36(b.id))
    process_orders()
    assert_equal(250, b.size)
end

function test_build_harbour()
-- try to reproduce mantis bug 2221
    local r = region.create(0, 0, "plain")
    local f = faction.create("human", "harbour@eressea.de", "de")
    local u = unit.create(f, r)
    size = 30
    u.number = 20
    u:set_skill("building", 3)
    u:add_item("money", size*250)
    u:add_item("stone", size*5)
    u:add_item("log", size*5)
    u:clear_orders()
    u:add_order("MACHE HAFEN")
    process_orders()
    assert_not_nil(u.building)
    assert_equal("harbour", u.building.type)
    assert_equal("Hafen", u.building.name)
    assert_equal(20, u.building.size)
    process_orders()
    assert_equal(25, u.building.size)
    process_orders()
    assert_equal(25, u.building.size)
end

function test_caravan()
    local r = region.create(0,0, "desert")
    local r2 = region.create(1,0, "plain")
    local f = faction.create("human")
    local u = unit.create(f, r, 10)
    u:set_skill("building", 3)
    u:set_skill("roadwork", 10)
    u:add_item("money", 8000)
    u:add_item("horse", 2)
    u:add_item("stone", 210)
    u:add_item("log", 50)
    u:add_item("iron", 10)
    u:set_orders("MACHE 100 STRASSE OST")
    process_orders()
    assert_equal(0, r.roads[3]) -- keine Arme, keine Kekse
    u:set_orders("MACHE 1 Karawanserei\nBEZAHLE NICHT")
    process_orders()
    assert_not_nil(u.building)
    assert_equal(1, u.building.size)
    u:set_orders("MACHE 100 STRASSE OST\nBEZAHLE NICHT")
    process_orders()
    assert_equal(0, r.roads[3]) -- Karawane ist noch nicht fertig
    u:set_orders("MACHE 9 Karawanserei\nBEZAHLE NICHT")
    process_orders()
    assert_equal(10, u.building.size)
    u:set_orders("MACHE 100 STRASSE OST\nBEZAHLE NICHT")
    process_orders()
    assert_equal(0, r.roads[3]) -- Karawane ist noch nicht fertig
    u:set_orders("MACHE 200 STRASSE OST")
    process_orders()
    assert_equal(100, r.roads[3]) -- maximaler Ausbau Strasse in W�sten
    assert_equal(100, u:get_item('stone'))
    u:set_orders("ZERSTOERE 1")
    process_orders()
    assert_equal(9, u.building.size)
    assert_equal(100, r.roads[3])
    u:set_orders("ZERSTOERE 9")
    process_orders()
    assert_nil(u.building)
    assert_equal(50, r.roads[3])
end
