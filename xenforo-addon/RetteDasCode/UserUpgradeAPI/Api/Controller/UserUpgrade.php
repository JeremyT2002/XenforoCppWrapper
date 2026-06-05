<?php

namespace RetteDasCode\UserUpgradeAPI\Api\Controller;

use XF\Api\Controller\AbstractController;
use XF\Mvc\ParameterBag;

// GET /api/user-upgrades/{user_id}
class UserUpgrade extends AbstractController
{
    public function actionGet(ParameterBag $params)
    {
        $this->assertApiScopeByRequestMethod('user');

        $userId = $params->user_id ?: $this->filter('user_id', 'uint');
        if (!$userId)
        {
            return $this->notFound(\XF::phrase('requested_user_not_found'));
        }

        /** @var \XF\Entity\User $user */
        $user = $this->em()->find('XF:User', $userId);
        if (!$user)
        {
            return $this->notFound(\XF::phrase('requested_user_not_found'));
        }

        $activeRecords = $this->finder('XF:UserUpgradeActive')
            ->where('user_id', $userId)
            ->with('UserUpgrade')
            ->order('end_date')
            ->fetch();

        $now = \XF::$time;

        $upgrades = [];
        foreach ($activeRecords AS $record)
        {
            /** @var \XF\Entity\UserUpgrade $upgrade */
            $upgrade = $record->UserUpgrade;

            $endDate = (int) $record->end_date;
            $isPermanent = ($endDate === 0);
            $remaining = $isPermanent ? null : max(0, $endDate - $now);

            $upgrades[] = [
                'user_upgrade_id'        => (int) $record->user_upgrade_id,
                'user_upgrade_record_id' => (int) $record->user_upgrade_record_id,
                'title'                  => $upgrade ? $upgrade->title : '',
                'start_date'             => (int) $record->start_date,
                'end_date'               => $endDate,
                'is_permanent'           => $isPermanent,
                'expired'                => $isPermanent ? false : ($endDate <= $now),
                'remaining_seconds'      => $remaining,
                'remaining_days'         => $isPermanent ? null : (int) floor($remaining / 86400),
                'remaining_human'        => $this->formatRemaining($endDate, $now),
            ];
        }

        return $this->apiResult([
            'user_id'  => (int) $userId,
            'now'      => $now,
            'upgrades' => $upgrades,
        ]);
    }

    protected function formatRemaining(int $endDate, int $now): string
    {
        if ($endDate === 0)
        {
            return 'permanent';
        }

        $secs = $endDate - $now;
        if ($secs <= 0)
        {
            return 'expired';
        }

        $days = intdiv($secs, 86400);
        $hours = intdiv($secs % 86400, 3600);
        $minutes = intdiv($secs % 3600, 60);

        if ($days > 0)
        {
            return $hours > 0 ? "{$days}d {$hours}h" : "{$days}d";
        }
        if ($hours > 0)
        {
            return $minutes > 0 ? "{$hours}h {$minutes}m" : "{$hours}h";
        }
        return "{$minutes}m";
    }
}
