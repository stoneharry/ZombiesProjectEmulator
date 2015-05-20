ALTER TABLE `characters`
	ADD COLUMN `deathsLeft` TINYINT(3) UNSIGNED NOT NULL DEFAULT '3' AFTER `grantableLevels`;
