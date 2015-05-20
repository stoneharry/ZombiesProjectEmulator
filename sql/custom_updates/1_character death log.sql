CREATE TABLE `character_death_log` (
 `time` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
 `guid` INT UNSIGNED NOT NULL,
 `name` VARCHAR(255) NOT NULL,
 `ip` VARCHAR(255) NOT NULL,
 `causeLog` VARCHAR(1000) NOT NULL
)
COMMENT='Logs for the death system.'
COLLATE='utf8_general_ci'
ENGINE=InnoDB;