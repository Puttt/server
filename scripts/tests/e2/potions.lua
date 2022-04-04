local tcname = 'tests.e2.potions'
local lunit = require("lunit")
if _VERSION >= 'Lua 5.2' then
  _ENV = module(tcname, 'seeall')
else
  module(tcname, lunit.testcase, package.seeall)
end

function setup()
    eressea.free_game()
    eressea.settings.set("nmr.timeout", "0")
    eressea.settings.set("NewbieImmunity", "0")
    eressea.settings.set("rules.food.flags", "4")
    eressea.settings.set("rules.ship.storms", "0")
    eressea.settings.set("magic.regeneration.enable", "0")
end

function test_use_goliathwater()
    local r1 = region.create(0, 0, "plain")
    local r2 = region.create(1, 0, "plain")
    local f = faction.create("human")
    local u = unit.create(f, r1, 1)
    u.number = 10
    u:add_item('goliathwater', 2)
    u:add_item('money', 10 * 2000)
    u:add_order("BENUTZEN 1 Goliathwasser")
    u:add_order("NACH O")
    process_orders()
    assert_equal(1, u:get_item('goliathwater'))
    assert_equal(r2, u.region)
    u:clear_orders()
    u:add_order("BENUTZEN 1 Goliathwasser")
    u:add_order("NACH W")
    u:add_item('money', 1)
    process_orders()
    assert_equal(0, u:get_item('goliathwater'))
    assert_equal(r2, u.region)
end

function test_use_goliathwater_limited()
    local r1 = region.create(0, 0, "plain")
    local r2 = region.create(1, 0, "plain")
    local f = faction.create("human")
    local u = unit.create(f, r1, 1)
    u.number = 20
    u:add_item('goliathwater', 2)
    u:add_item('money', 10 * 2000 + 10 * 540)
    u:add_order("BENUTZEN 1 Goliathwasser")
    u:add_order("NACH O")
    process_orders()
    assert_equal(1, u:get_item('goliathwater'))
    assert_equal(r2, u.region)
    u:clear_orders()
    u:add_order("BENUTZEN 1 Goliathwasser")
    u:add_order("NACH W")
    u:add_item('money', 1)
    process_orders()
    assert_equal(0, u:get_item('goliathwater'))
    assert_equal(r2, u.region)
end

function test_use_goliathwater_trolls()
    local r1 = region.create(0, 0, "plain")
    local r2 = region.create(1, 0, "plain")
    local f = faction.create("troll")
    local u = unit.create(f, r1, 1)
    u.number = 10
    u:add_item('goliathwater', 2)
    u:add_item('money', 10 * 2000)
    u:add_order("BENUTZEN 1 Goliathwasser")
    u:add_order("NACH O")
    process_orders()
    assert_equal(1, u:get_item('goliathwater'))
    assert_equal(r2, u.region)
    u:clear_orders()
    u:add_order("BENUTZEN 1 Goliathwasser")
    u:add_order("NACH W")
    u:add_item('money', 1)
    process_orders()
    assert_equal(0, u:get_item('goliathwater'))
    assert_equal(r2, u.region)
end

function test_use_goliathwater_goblins()
    local r1 = region.create(0, 0, "plain")
    local r2 = region.create(1, 0, "plain")
    local f = faction.create("goblin")
    local u = unit.create(f, r1, 1)
    u.number = 10
    u:add_item('goliathwater', 2)
    u:add_item('money', 10 * 2000)
    u:add_order("BENUTZEN 1 Goliathwasser")
    u:add_order("NACH O")
    process_orders()
    assert_equal(1, u:get_item('goliathwater'))
    assert_equal(r2, u.region)
    u:clear_orders()
    u:add_order("BENUTZEN 1 Goliathwasser")
    u:add_order("NACH W")
    u:add_item('money', 1)
    process_orders()
    assert_equal(0, u:get_item('goliathwater'))
    assert_equal(r2, u.region)
end

