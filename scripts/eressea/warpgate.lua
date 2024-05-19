local warp = {}

local warps = {}

function warp.init()
    local r, b
    for r in regions() do
        local uid = r:get_key('warp')
        if uid then
            local target = get_region_by_id(uid)
            if target then
                table.insert(warps, {from = r, to = target})
            end
        end
    end
end

function move_all(from, to)
    -- move all units not in a ship
    local moved = false
    local units = {}
    for u in from.units do
        if u.ship == nil and u.faction ~= 666 then
            table.insert(units, u)
        end
    end
    for _, u in ipairs(units) do
        u.region = to
        moved = true
    end

    -- move all non-empty ships and units in them
    local ships = {}
    for s in from.ships do
        table.insert(ships, s)
    end
    for _, s in ipairs(ships) do
        local units = {}
        for u in s.units do
            table.insert(units, u)
            local msg = message.create('warpgate')
            msg:set_region('from', from)
            msg:set_region('to', to)
            msg:set_unit('unit', u)
            msg:send_faction(u.faction)
        end
        s.region = to
        moved = true
        s.coast = nil
        for _, u in ipairs(units) do
            u.region = to
            u.ship = s
        end
    end
    return moved
end

function warp.update()
    for _, w in ipairs(warps) do
        if move_all(w.from, w.to) then
            print("warp from " .. tostring(w.from) .. " to " .. tostring(w.to))
        end
    end
end

function warp.mark(r, destination)
    r:set_key('warp', destination.id)
end

function warp.gm_edit()
    local sel = { nil, nil }
    local r
    local i = 1
    local destination = gmtool.get_cursor()
    for r in gmtool.get_selection() do
        r:set_key('warp', destination.id)
    end
end

return warp
