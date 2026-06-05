<?php

namespace RetteDasCode\UserUpgradeAPI;

use XF\AddOn\AbstractSetup;
use XF\AddOn\StepRunnerInstallTrait;
use XF\AddOn\StepRunnerUpgradeTrait;
use XF\AddOn\StepRunnerUninstallTrait;

/**
 * This add-on does not create any database tables of its own. It only adds an
 * API route (see _data/routes.xml) and a controller that reads existing
 * XenForo user-upgrade data. The Setup class is provided so the add-on can be
 * installed/uninstalled cleanly from the Admin Control Panel.
 */
class Setup extends AbstractSetup
{
    use StepRunnerInstallTrait;
    use StepRunnerUpgradeTrait;
    use StepRunnerUninstallTrait;
}
