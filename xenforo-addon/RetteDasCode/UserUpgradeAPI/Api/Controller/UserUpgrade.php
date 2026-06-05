<?php

namespace RetteDasCode\UserUpgradeAPI\Api\Controller;

use XF\Api\Controller\AbstractController;
use XF\Mvc\ParameterBag;

/**
 * GET /api/user-upgrades/{user_id}
 *
 * Returns the active user upgrades for the given user, including their
 * start_date and end_date so external apps can compute the remaining duration.
 *
 * Requires the `user:read` scope. As with all sensitive lookups, call this with
 * a super user key from a trusted backend only -- never expose the key to a
 * client application.
 */
class UserUpgrade extends AbstractController
{
    public function actionGet(ParameterBag $params)
    {
        // For a GET request this checks the `user:read` scope.
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

        // Currently active upgrades (XenForo moves expired ones out of this
        // table into xf_user_upgrade_expired).
        $activeRecords = $this->finder('XF:UserUpgradeActive')
            ->where('user_id', $userId)
            ->with('UserUpgrade')
            ->order('end_date')
            ->fetch();

        $upgrades = [];
        foreach ($activeRecords AS $record)
        {
            /** @var \XF\Entity\UserUpgrade $upgrade */
            $upgrade = $record->UserUpgrade;

            $upgrades[] = [
                'user_upgrade_id'        => (int) $record->user_upgrade_id,
                'user_upgrade_record_id' => (int) $record->user_upgrade_record_id,
                'title'                  => $upgrade ? $upgrade->title : '',
                'start_date'             => (int) $record->start_date,
                'end_date'               => (int) $record->end_date, // 0 == permanent
            ];
        }

        return $this->apiResult([
            'user_id'  => (int) $userId,
            'upgrades' => $upgrades,
        ]);
    }
}
