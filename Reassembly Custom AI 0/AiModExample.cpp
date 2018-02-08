#include "game/StdAfx.h"
#include "AiModExample.h"

#include <game/AI.h>
#include <game/Blocks.h>

struct AAvoidCluster_ModStarter final : public AIAction
{

    vector<Obstacle> obstacles;
    snConfig         config;

    static bool supportsConfig(const AICommandConfig& cfg) { return cfg.isMobile; }

    void generateClusterObstacleList(Block* command)
    {
        // FIXME this should include asteroids! - nearby asteroid blocks?
        const BlockCluster *cl       = command->cluster;
        const float         clRad    = cl->getBRadius();
        const float2        clPos    = cl->getAbsolutePos();
        const float2        clVel    = cl->getVel();
        const BlockList    &commands = m_ai->getAllies();

        obstacles.clear();
        foreach (const Block *bl, commands)
        {
            const BlockCluster *bcl = bl->cluster;
            if (bl != command && cl != bcl && bcl->getMass() > 0.5f * cl->getMass())
            {
                Obstacle obs(*bcl, clRad, bcl->getMass() / cl->getMass());
                if (intersectRayCircle(clPos, clVel - obs.vel, obs.pos, 5 * obs.rad))
                    obstacles.push_back(obs);
            }
        }
    }

    AAvoidCluster_ModStarter(AI* ai) : AIAction(ai, LANE_MOVEMENT, PRI_ALWAYS) {}

    virtual uint update(uint blockedLanes)
    {
        // explosive blocks / missiles don't collide with each other anyway
        if (m_ai->command->sb.features&Block::EXPLODE)
            return LANE_NONE;

        // don't bother for single seeds
        if ((m_ai->command->sb.features&Block::SEED) && getCluster()->size() == 1)
            return LANE_NONE;

        generateClusterObstacleList(m_ai->command);

        if (velocityObstacles(&config.velocity, NULL, getClusterPos(), getCluster()->getVel(),
            m_ai->nav->maxAccel, getTargetDirection(m_ai, obstacles), float2(), obstacles))
        {
            m_ai->nav->setDest(config, SN_VELOCITY, 0);
            return LANE_MOVEMENT;
        }
        return LANE_NONE;
    }

    string toStringName() const override { return "AAvoidCluster_ModStarter"; }
};


// set fallback target - i.e. incoming missile
struct AFallbackTarget_ModStarter final : public AIAction
{

    static bool supportsConfig(const AICommandConfig& cfg) { return cfg.hasWeapons; }
    AFallbackTarget_ModStarter(AI* ai) : AIAction(ai, LANE_TARGET, PRI_ALWAYS) {}

    virtual uint update(uint blockedLanes)
    {
        float2 blah;
        if (m_ai->isBigUpdate())
        {
            float         minDistSqr = FLT_MAX;
            const Block  *target     = NULL;
            /*
            const float2  pos        = m_ai->command->getClusterPos();

            foreach (const Block* tgt, m_ai->getEnemies())
            {
                const float dist = distanceSqr(pos, tgt->getAbsolutePos());
                if (dist < minDistSqr && (isActivelyHostile(tgt) != AI::NEUTRAL)) {
                    minDistSqr = dist;
                    target     = tgt;
                }
            }
            foreach (const Block *tgt, m_ai->getQuery().queryBlockObstacles(m_ai->command))
            {
                const float dist = distanceSqr(pos, tgt->getAbsolutePos());
                if (dist < minDistSqr && (isActivelyHostile(tgt) != AI::NEUTRAL)) {
                    minDistSqr = dist;
                    target     = tgt;
                }
            }

            status = target ? "Set fallback target" : "no fallback target";
            m_ai->fallbackTarget = target;
            */
        }
        return LANE_NONE;       // never blocks other targetingp
    }

    string toStringName() const override { return "AFallbackTarget_ModStarter"; }
};


struct AWeapons_ModStarter final : AIAction
{
    //*
    int  enabled = 0;        // number of weapons enabled
    bool isFallback = false;

    /*
    static bool supportsConfig(const AICommandConfig& cfg)
    {
        return cfg.hasWeapons && (cfg.features&FIREABLE_WEAPONS);
    }
    */

    AWeapons_ModStarter(AI* ai) : AIAction(ai, LANE_SHOOT) {}

    uint update(uint blockedLanes) override
    {
        blockedLanes;
        enabled = 0;
        if (m_ai->target) {
            FiringData data(m_ai->target);
            data.filter = ~Block::AUTOFIRE;
            enabled = m_ai->fireWeaponsAt(data);
            isFallback = false;
        }
        if (!enabled && m_ai->fallbackTarget && nearZero(m_ai->rushDir)) {
            FiringData data(m_ai->fallbackTarget);
            data.filter = ~Block::AUTOFIRE;
            enabled = m_ai->fireWeaponsAt(data);
            isFallback = true;
        }
        return enabled ? LANE_SHOOT : LANE_NONE;
    }

    string toStringEx() const override
    {
        //return str_format("enabled %d at %s target", enabled, isFallback ? "fallback" : "main");
        //return "AWeaponsExample";
        string result("enabled ");
        result += enabled;
        result += " at ";
        result += isFallback ? "fallback" : "main";
        result += " target";
        return result;
    }
    //*/

    string toStringName() const override { return "AWeapons_ModStarter"; }
};


// target based on faction: attack enemy ships
struct ATargetEnemy_ModStarter final : public ATargetBase {

    ATargetEnemy_ModStarter(AI* ai) : ATargetBase(ai) { }

    virtual uint update(uint blockedLanes)
    {
        return findSetTarget();
    }

    // copy-paste from Reassembly ATargetBase.
    // If you don't implement it, you get the built-in version instead.
    // If you use the built-in version, the two copy-paste "CVars" below
    // are respected (instead of being hard-coded as below).
    #if 1
    uint findSetTarget()
    {
        // cvars in Reassembly; lazy way to get them here
        int aiTargetMin = 500;
        float aiTargetThreshold = 0.25f;

        if (!m_ai->getAttackCaps().weapons)
        {
            status = "No Weapons";
            m_ai->setTarget(NULL, AIMood::NEUTRAL);
            return LANE_TARGET;
        }

        Target target = testAcceptTarget(m_ai->priorityTarget.get());
        if (!target.first)
            target = testAcceptTarget(m_ai->target.get());

        if (!(target.first && m_ai->priorityTarget) && m_ai->isBigUpdate())
        {
            targets.clear();
            const bool isAttack = (m_ai->getConfig().flags&ECommandFlags::ATTACK);
            const int deadlyThreshold = isAttack ? 10 : min(aiTargetMin, int(aiTargetThreshold * getCluster()->getDeadliness()));
            foreach (const Block* tgt, m_ai->getEnemies())
            {
                AIMood mood = AIMood::NEUTRAL;
                if (tgt->getBlueprintDeadliness() >= deadlyThreshold &&
                    (!isAttack || !tgt->sb.isTransient()) &&
                    (mood = acceptTarget(tgt)) != AI::NEUTRAL)
                {
                    targets.push_back(make_pair(tgt, mood));
                }
            }

            // pick closest remaining target
            const float2 defPos = !nearZero(m_ai->defendPos) ? m_ai->defendPos : getClusterPos();
            target = vec_min(
                targets, [&](const Target& tgt) { return targetDistanceMetric(defPos, tgt.first); },
                target, (target.first ? (0.7 * targetDistanceMetric(defPos, target.first)) :
                    FLT_MAX));
        }

        if (!target.first)
            return noAction("No Target");

        status = "Found Target";
        m_ai->setTarget(target.first, target.second);
        return LANE_TARGET;
    }
    #endif

    string toStringName() const override { return "ATargetEnemy_ModStarter"; }
};


//=============================================================================
// Exported functions
//=============================================================================
/*
bool SupportsConfig(const char * name, const AICommandConfig& cfg)
{
    if (!std::strcmpi(name, "AWeapons"))
        return AWeaponsExample::supportsConfig(cfg);
    return false;
}
*/

AIAction * CreateAiAction(const char * name, AI* ai) {
    if (!_strcmpi(name, "AAvoidCluster"))
        return new AAvoidCluster_ModStarter(ai);
    if (!_strcmpi(name, "AFallbackTarget"))
        return new AFallbackTarget_ModStarter(ai);
    if (!_strcmpi(name, "ATargetEnemy"))
        return new ATargetEnemy_ModStarter(ai);
    if (!_strcmpi(name, "AWeapons"))
        return new AWeapons_ModStarter(ai);
    return nullptr;
}
