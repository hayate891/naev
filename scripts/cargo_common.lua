

function cargo_calculateRoute ()
    
    -- Note: this mission does not make any system claims. 
    origin_p, origin_s = planet.cur()
    local routesys = origin_s
    local routepos = origin_p:pos()
    
    -- Select mission tier.
    local tier = rnd.rnd(0, 4)
    
    -- Find an inhabited planet 0-3 jumps away.
    -- Farther distances have a lower chance of appearing.
    
    local seed = rnd.rnd()
    if     seed < 0.30 then missdist = 0
    elseif seed < 0.60 then missdist = 1
    elseif seed < 0.80 then missdist = 2
    else                    missdist = 3
    end
    
    local planets = {}
    getsysatdistance(system.cur(), missdist, missdist,
        function(s)
            for i, v in ipairs(s:planets()) do
                if v:services()["inhabited"] and v ~= planet.cur() and v:class() ~= 0 and
                        not (s==system.cur() and ( vec2.dist( v:pos(), routepos ) < 2500 ) ) then
                    planets[#planets + 1] = {v, s}
                end
           end
           return true
        end)

    if #planets == 0 then
        abort()
    end
    
    index = rnd.rnd(1, #planets)
    destplanet = planets[index][1]
    destsys = planets[index][2]
    
    -- We have a destination, now we need to calculate how far away it is by simulating the journey there.
    -- Assume shortest route with no interruptions.
    -- This is used to calculate the reward.
    traveldist = 0
    numjumps = origin_s:jumpDist(destsys)
    
    while routesys ~= destsys do
        -- We're not in the destination system yet.
        -- So, get the next system on the route, and the distance between our entry point and the jump point to the next system.
        -- Then, set the exit jump point as the next entry point.
        local tempsys = getNextSystem(routesys, destsys)
        traveldist = traveldist + vec2.dist(routepos, routesys:jumpPos(tempsys))
        routepos = tempsys:jumpPos(routesys)
        routesys = tempsys
    end
    -- We ARE in the destination system now, so route from the entry point to the destination planet.
    traveldist = traveldist + vec2.dist(routepos, destplanet:pos())
    
    -- We now know where. But we don't know what yet. Randomly choose a commodity type.
    -- TODO: I'm using the standard cargo types for now, but this should be changed to custom cargo once local-defined commodities are implemented.
    local cargoes = {"Food", "Industrial Goods", "Medicine", "Luxury Goods", "Ore"}
    cargo = cargoes[rnd.rnd(1, #cargoes)]

    -- Return lots of stuff
    return destplanet, destsys, numdist, traveldist, cargo, tier
end


-- Choose the next system to jump to on the route from system nowsys to system finalsys.
function getNextSystem(nowsys, finalsys)
    if nowsys == finalsys then
        return nowsys
    else
        local neighs = nowsys:adjacentSystems()
        local nearest = -1
        local mynextsys = finalsys
        for _, j in pairs(neighs) do
            if nearest == -1 or j:jumpDist(finalsys) < nearest then
                nearest = j:jumpDist(finalsys)
                mynextsys = j
            end
        end
        return mynextsys
    end
end 

