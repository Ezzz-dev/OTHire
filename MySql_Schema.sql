-- phpMyAdmin SQL Dump
-- version 4.0.5
-- http://www.phpmyadmin.net
--
-- Host: 127.0.0.1:3306

-- Generation Time: Mar 28, 2014 at 10:45 PM
-- Server version: 5.5.33
-- PHP Version: 5.4.19

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;

--
-- Database: `newotserv`
--

-- --------------------------------------------------------

--
-- Table structure for table `accounts`
--

CREATE TABLE `accounts` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `password` varchar(255) NOT NULL,
  `email` varchar(255) NOT NULL DEFAULT '',
  `premend` int(10) unsigned NOT NULL DEFAULT '0',
  `blocked` tinyint(1) NOT NULL DEFAULT '0',
  `warnings` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB  DEFAULT CHARSET=utf8 AUTO_INCREMENT=123457 ;

--
-- Dumping data for table `accounts`
--

INSERT INTO `accounts` (`id`, `password`, `email`, `premend`, `blocked`, `warnings`) VALUES
(123456, '123456', '', 0, 0, 0);

--
-- Triggers `accounts`
--
DROP TRIGGER IF EXISTS `ondelete_accounts`;
DELIMITER //
CREATE TRIGGER `ondelete_accounts` BEFORE DELETE ON `accounts`
 FOR EACH ROW BEGIN
	DELETE FROM `bans` WHERE `type` = 3 AND `value` = OLD.`id`;
END
//
DELIMITER ;

-- --------------------------------------------------------

--
-- Table structure for table `bans`
--

CREATE TABLE `bans` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `type` int(11) NOT NULL COMMENT 'this field defines if its ip, account, player, or any else ban',
  `value` int(10) unsigned NOT NULL COMMENT 'ip, player guid, account number',
  `param` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'mask',
  `active` tinyint(1) NOT NULL DEFAULT '1',
  `expires` int(11) NOT NULL,
  `added` int(10) unsigned NOT NULL,
  `admin_id` int(10) unsigned DEFAULT NULL,
  `comment` varchar(1024) NOT NULL DEFAULT '',
  `reason` int(10) unsigned NOT NULL DEFAULT '0',
  `action` int(10) unsigned NOT NULL DEFAULT '0',
  `statement` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `type` (`type`,`value`),
  KEY `expires` (`expires`),
  KEY `admin_id` (`admin_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `environment_killers`
--

CREATE TABLE `environment_killers` (
  `kill_id` int(10) unsigned NOT NULL,
  `name` varchar(255) NOT NULL,
  PRIMARY KEY (`kill_id`,`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Dumping data for table `environment_killers`
--

INSERT INTO `environment_killers` (`kill_id`, `name`) VALUES
(6, 'a orc'),
(18, 'a rat'),
(19, 'a rat');

-- --------------------------------------------------------

--
-- Table structure for table `global_storage`
--

CREATE TABLE `global_storage` (
  `key` int(10) unsigned NOT NULL,
  `value` int(11) NOT NULL,
  PRIMARY KEY (`key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `groups`
--

CREATE TABLE `groups` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL COMMENT 'group name',
  `flags` bigint(20) unsigned NOT NULL DEFAULT '0',
  `access` int(11) NOT NULL DEFAULT '0',
  `violation` int(11) NOT NULL DEFAULT '0',
  `maxdepotitems` int(11) NOT NULL,
  `maxviplist` int(11) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB  DEFAULT CHARSET=utf8 AUTO_INCREMENT=6 ;

--
-- Dumping data for table `groups`
--

INSERT INTO `groups` (`id`, `name`, `flags`, `access`, `violation`, `maxdepotitems`, `maxviplist`) VALUES
(1, 'Player', 0, 0, 0, 2000, 100),
(2, 'Tutor', 16777216, 0, 0, 2000, 100),
(3, 'Senior Tutor', 8796109897728, 0, 0, 2000, 100),
(4, 'Gamemaster', 15811415236879, 2, 0, 1000, 100),
(5, 'God', 1096894357496, 3, 0, 2000, 100);

-- --------------------------------------------------------

--
-- Table structure for table `guilds`
--

CREATE TABLE `guilds` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  `owner_id` int(10) unsigned NOT NULL,
  `creationdate` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`),
  KEY `owner_id` (`owner_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

--
-- Triggers `guilds`
--
DROP TRIGGER IF EXISTS `oncreate_guilds`;
DELIMITER //
CREATE TRIGGER `oncreate_guilds` AFTER INSERT ON `guilds`
 FOR EACH ROW BEGIN
	INSERT INTO `guild_ranks` (`name`, `level`, `guild_id`) VALUES ('Leader', 3, NEW.`id`);
	INSERT INTO `guild_ranks` (`name`, `level`, `guild_id`) VALUES ('Vice-Leader', 2, NEW.`id`);
	INSERT INTO `guild_ranks` (`name`, `level`, `guild_id`) VALUES ('Member', 1, NEW.`id`);
END
//
DELIMITER ;

-- --------------------------------------------------------

--
-- Table structure for table `guild_invites`
--

CREATE TABLE `guild_invites` (
  `player_id` int(10) unsigned NOT NULL,
  `guild_id` int(10) unsigned NOT NULL COMMENT 'guild',
  UNIQUE KEY `player_id` (`player_id`),
  KEY `guild_id` (`guild_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `guild_members`
--

CREATE TABLE `guild_members` (
  `player_id` int(10) unsigned NOT NULL COMMENT 'if you doesnt use new guild system you are free to delete this table',
  `rank_id` int(10) unsigned NOT NULL COMMENT 'a rank which belongs to certain guild',
  `nick` varchar(255) NOT NULL DEFAULT '',
  UNIQUE KEY `player_id` (`player_id`),
  KEY `rank_id` (`rank_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `guild_ranks`
--

CREATE TABLE `guild_ranks` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `guild_id` int(10) unsigned NOT NULL COMMENT 'guild',
  `name` varchar(255) NOT NULL COMMENT 'rank name',
  `level` int(11) NOT NULL COMMENT 'rank level - leader, vice leader, member, maybe something else',
  PRIMARY KEY (`id`),
  KEY `guild_id` (`guild_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `houses`
--

CREATE TABLE `houses` (
  `id` int(10) unsigned NOT NULL,
  `townid` int(10) unsigned NOT NULL DEFAULT '0',
  `name` varchar(100) NOT NULL,
  `rent` int(10) unsigned NOT NULL DEFAULT '0',
  `guildhall` tinyint(1) NOT NULL DEFAULT '0',
  `tiles` int(10) unsigned NOT NULL DEFAULT '0',
  `doors` int(10) unsigned NOT NULL DEFAULT '0',
  `beds` int(10) unsigned NOT NULL DEFAULT '0',
  `owner` int(11) NOT NULL DEFAULT '0',
  `paid` int(10) unsigned NOT NULL DEFAULT '0',
  `clear` tinyint(1) NOT NULL DEFAULT '0',
  `warnings` int(11) NOT NULL DEFAULT '0',
  `lastwarning` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Dumping data for table `houses`
--

INSERT INTO `houses` (`id`, `townid`, `name`, `rent`, `guildhall`, `tiles`, `doors`, `beds`, `owner`, `paid`, `clear`, `warnings`, `lastwarning`) VALUES
(1, 1, 'Main Street 2a', 1500, 0, 40, 1, 2, 0, 0, 0, 0, 0),
(2, 1, 'Main street 1a', 1500, 0, 38, 1, 2, 0, 0, 0, 0, 0),
(3, 1, 'Main street 1b', 1500, 0, 40, 1, 2, 0, 0, 0, 0, 0),
(4, 1, 'Main street 1c', 1500, 0, 40, 1, 2, 0, 0, 0, 0, 0),
(5, 1, 'Main Street 2b', 1500, 0, 40, 1, 2, 0, 0, 0, 0, 0),
(6, 1, 'Main Street 2c', 1500, 0, 40, 1, 2, 0, 0, 0, 0, 0),
(7, 1, 'Shop 1', 1100, 0, 44, 2, 1, 0, 0, 0, 0, 0),
(8, 1, 'Boat Stream 1', 1200, 0, 28, 1, 1, 0, 0, 0, 0, 0),
(9, 1, 'Main street 3a', 1500, 0, 31, 1, 1, 0, 0, 0, 0, 0),
(10, 1, 'Main street 3b', 1500, 0, 32, 1, 1, 0, 0, 0, 0, 0),
(11, 1, 'Main street 3d', 1500, 0, 31, 1, 1, 0, 0, 0, 0, 0),
(12, 1, 'Main street 3c', 1500, 0, 32, 1, 1, 0, 0, 0, 0, 0),
(13, 1, 'Boat Stream 2', 1200, 0, 22, 1, 1, 0, 0, 0, 0, 0),
(14, 1, 'Shop 2a', 600, 0, 14, 1, 1, 0, 0, 0, 0, 0),
(15, 1, 'Shop 2b', 900, 0, 19, 1, 1, 0, 0, 0, 0, 0),
(16, 1, 'Shop 2c', 1000, 0, 17, 1, 1, 0, 0, 0, 0, 0),
(17, 1, 'Shop 2d', 1000, 0, 18, 1, 1, 0, 0, 0, 0, 0),
(18, 1, 'Sea View 1', 1300, 0, 36, 1, 1, 0, 0, 0, 0, 0),
(19, 1, 'Sea View 2', 1300, 0, 36, 1, 1, 0, 0, 0, 0, 0),
(20, 1, 'Pleasant Home 1', 1100, 0, 36, 1, 2, 0, 0, 0, 0, 0),
(21, 1, 'Pleasant Home 2', 1100, 0, 33, 1, 2, 0, 0, 0, 0, 0),
(22, 1, 'Smally Wally 1', 400, 0, 10, 1, 1, 0, 0, 0, 0, 0),
(23, 1, 'Smally Wally 2', 400, 0, 10, 1, 1, 0, 0, 0, 0, 0),
(24, 1, 'Smally Wally 3', 400, 0, 10, 1, 1, 0, 0, 0, 0, 0),
(25, 1, 'Smally Wally 4', 400, 0, 10, 1, 1, 0, 0, 0, 0, 0),
(26, 1, 'Tharkasian Road 1', 900, 0, 18, 1, 1, 0, 0, 0, 0, 0),
(27, 1, 'Tharkasian Road 2', 1300, 0, 30, 1, 2, 0, 0, 0, 0, 0),
(28, 1, 'Tharkas Guildhall 2', 7200, 0, 383, 17, 18, 0, 0, 0, 0, 0),
(29, 1, 'Tharkas Guildhall 1', 5400, 0, 201, 10, 11, 0, 0, 0, 0, 0),
(30, 1, 'Tharkas Farm 1', 1350, 0, 33, 2, 1, 0, 0, 0, 0, 0),
(31, 1, 'Tharkas Farm 2', 1350, 0, 33, 2, 1, 0, 0, 0, 0, 0),
(32, 2, 'Camel Avenue 1', 1200, 0, 50, 1, 1, 0, 0, 0, 0, 0),
(33, 2, 'Camel Avenue 2', 800, 0, 41, 1, 1, 0, 0, 0, 0, 0),
(34, 2, 'Camel Avenue 3', 1200, 0, 55, 2, 1, 0, 0, 0, 0, 0),
(35, 2, 'Camel Avenue 4', 500, 0, 21, 1, 1, 0, 0, 0, 0, 0),
(36, 2, 'Camel Avenue 5', 500, 0, 21, 1, 1, 0, 0, 0, 0, 0),
(37, 2, 'Camel Avenue 6', 500, 0, 21, 1, 1, 0, 0, 0, 0, 0),
(38, 2, 'Camel Avenue 7', 500, 0, 21, 1, 1, 0, 0, 0, 0, 0),
(39, 2, 'Camel Avenue 8', 500, 0, 21, 1, 1, 0, 0, 0, 0, 0),
(40, 2, 'Camel Avenue 9', 500, 0, 21, 1, 1, 0, 0, 0, 0, 0),
(41, 2, 'Camel Avenue 10', 2000, 0, 84, 1, 2, 0, 0, 0, 0, 0),
(42, 2, 'Oasis Road 1', 2100, 0, 78, 1, 2, 0, 0, 0, 0, 0),
(43, 2, 'Oasis Road 2', 2100, 0, 86, 1, 2, 0, 0, 0, 0, 0),
(44, 2, 'Oasis Road 3', 2100, 0, 82, 1, 2, 0, 0, 0, 0, 0),
(45, 2, 'Oasis Guildhall', 6000, 0, 229, 6, 10, 0, 0, 0, 0, 0),
(46, 2, 'Oasis Road 4', 1600, 0, 79, 2, 3, 0, 0, 0, 0, 0),
(47, 2, 'Oasis Road 5', 1700, 0, 93, 2, 2, 0, 0, 0, 0, 0),
(48, 2, 'Oasis Road 6', 1400, 0, 71, 1, 1, 0, 0, 0, 0, 0),
(49, 2, 'Oasis Road 7', 900, 0, 40, 1, 1, 0, 0, 0, 0, 0),
(50, 2, 'Oasis Road 8', 900, 0, 49, 1, 1, 0, 0, 0, 0, 0),
(51, 2, 'Oasis Road 9', 900, 0, 51, 1, 1, 0, 0, 0, 0, 0),
(52, 2, 'Oasis Road 10', 900, 0, 44, 1, 1, 0, 0, 0, 0, 0),
(53, 2, 'Barrancas Street 1', 1100, 0, 55, 0, 2, 0, 0, 0, 0, 0),
(54, 2, 'Barrancas Street 2', 1650, 0, 67, 0, 2, 0, 0, 0, 0, 0),
(55, 2, 'Barrancas Street 3', 850, 0, 43, 0, 1, 0, 0, 0, 0, 0);

-- --------------------------------------------------------

--
-- Table structure for table `house_auctions`
--

CREATE TABLE `house_auctions` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `house_id` int(10) unsigned NOT NULL,
  `player_id` int(10) unsigned NOT NULL,
  `bid` int(10) unsigned NOT NULL DEFAULT '0',
  `limit` int(10) unsigned NOT NULL DEFAULT '0',
  `endtime` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `house_id` (`house_id`),
  KEY `player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `house_lists`
--

CREATE TABLE `house_lists` (
  `house_id` int(10) unsigned NOT NULL,
  `listid` int(11) NOT NULL,
  `list` text NOT NULL,
  KEY `house_id` (`house_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `killers`
--

CREATE TABLE `killers` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `death_id` int(10) unsigned NOT NULL,
  `final_hit` tinyint(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`),
  KEY `death_id` (`death_id`)
) ENGINE=InnoDB  DEFAULT CHARSET=utf8 AUTO_INCREMENT=20 ;

--
-- Dumping data for table `killers`
--

INSERT INTO `killers` (`id`, `death_id`, `final_hit`) VALUES
(2, 2, 1),
(3, 3, 1),
(6, 6, 1),
(7, 7, 1),
(8, 7, 0),
(9, 7, 0),
(10, 8, 1),
(11, 8, 0),
(12, 9, 1),
(13, 10, 1),
(14, 11, 1),
(15, 12, 1),
(16, 12, 0),
(17, 13, 1),
(18, 14, 1),
(19, 14, 0);

-- --------------------------------------------------------

--
-- Table structure for table `map_store`
--

CREATE TABLE `map_store` (
  `house_id` int(10) unsigned NOT NULL,
  `data` longblob NOT NULL,
  KEY `house_id` (`house_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `players`
--

CREATE TABLE `players` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  `account_id` int(10) unsigned NOT NULL,
  `group_id` int(10) unsigned NOT NULL COMMENT 'users group',
  `sex` int(10) unsigned NOT NULL DEFAULT '0',
  `vocation` int(10) unsigned NOT NULL DEFAULT '0',
  `experience` bigint(20) unsigned NOT NULL DEFAULT '0',
  `level` int(10) unsigned NOT NULL DEFAULT '1',
  `maglevel` int(10) unsigned NOT NULL DEFAULT '0',
  `health` int(11) NOT NULL DEFAULT '100',
  `healthmax` int(11) NOT NULL DEFAULT '100',
  `mana` int(11) NOT NULL DEFAULT '100',
  `manamax` int(11) NOT NULL DEFAULT '100',
  `manaspent` int(10) unsigned NOT NULL DEFAULT '0',
  `soul` int(10) unsigned NOT NULL DEFAULT '0',
  `direction` int(10) unsigned NOT NULL DEFAULT '0',
  `lookbody` int(10) unsigned NOT NULL DEFAULT '10',
  `lookfeet` int(10) unsigned NOT NULL DEFAULT '10',
  `lookhead` int(10) unsigned NOT NULL DEFAULT '10',
  `looklegs` int(10) unsigned NOT NULL DEFAULT '10',
  `looktype` int(10) unsigned NOT NULL DEFAULT '136',
  `posx` int(11) NOT NULL DEFAULT '0',
  `posy` int(11) NOT NULL DEFAULT '0',
  `posz` int(11) NOT NULL DEFAULT '0',
  `cap` int(11) NOT NULL DEFAULT '0',
  `lastlogin` int(10) unsigned NOT NULL DEFAULT '0',
  `lastlogout` int(10) unsigned NOT NULL DEFAULT '0',
  `lastip` int(10) unsigned NOT NULL DEFAULT '0',
  `save` tinyint(1) NOT NULL DEFAULT '1',
  `conditions` blob NOT NULL COMMENT 'drunk, poisoned etc',
  `skull_type` int(11) NOT NULL DEFAULT '0',
  `skull_time` int(10) unsigned NOT NULL DEFAULT '0',
  `loss_experience` int(11) NOT NULL DEFAULT '100',
  `loss_mana` int(11) NOT NULL DEFAULT '100',
  `loss_skills` int(11) NOT NULL DEFAULT '100',
  `loss_items` int(11) NOT NULL DEFAULT '10',
  `loss_containers` int(11) NOT NULL DEFAULT '100',
  `town_id` int(11) NOT NULL COMMENT 'old masterpos, temple spawn point position',
  `balance` int(11) NOT NULL DEFAULT '0' COMMENT 'money balance of the player for houses paying',
  `online` tinyint(1) NOT NULL DEFAULT '0',
  `rank_id` int(11) NOT NULL COMMENT 'only if you use __OLD_GUILD_SYSTEM__',
  `guildnick` varchar(255) NOT NULL COMMENT 'only if you use __OLD_GUILD_SYSTEM__',
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`),
  KEY `online` (`online`),
  KEY `account_id` (`account_id`),
  KEY `group_id` (`group_id`)
) ENGINE=InnoDB  DEFAULT CHARSET=utf8 AUTO_INCREMENT=8 ;

--
-- Dumping data for table `players`
--

INSERT INTO `players` (`id`, `name`, `account_id`, `group_id`, `sex`, `vocation`, `experience`, `level`, `maglevel`, `health`, `healthmax`, `mana`, `manamax`, `manaspent`, `soul`, `direction`, `lookbody`, `lookfeet`, `lookhead`, `looklegs`, `looktype`, `posx`, `posy`, `posz`, `cap`, `lastlogin`, `lastlogout`, `lastip`, `save`, `conditions`, `skull_type`, `skull_time`, `loss_experience`, `loss_mana`, `loss_skills`, `loss_items`, `loss_containers`, `town_id`, `balance`, `online`, `rank_id`, `guildnick`) VALUES
(3, 'GM Ezzz', 123456, 5, 1, 0, 100, 2, 4, 100, 100, 100, 100, 70775, 0, 2, 10, 10, 10, 10, 75, 32368, 32215, 7, 10000, 1396046551, 1396045859, 400558280, 1, '', 0, 0, 100, 100, 100, 10, 100, 1, 0, 1, 0, ''),
(5, 'Eternal', 123456, 1, 0, 1, 15694800, 100, 100, 100, 100, 1000, 100, 0, 0, 3, 114, 95, 76, 114, 139, 32378, 32108, 7, 500, 1395972211, 1395972205, 16777343, 1, '', 0, 0, 100, 100, 100, 10, 100, 1, 0, 0, 0, ''),
(6, 'Noob', 123456, 1, 0, 1, 0, 100, 0, 100, 100, 1000, 100, 4294967295, 0, 0, 69, 95, 78, 97, 136, 32369, 32241, 7, 500, 1395972171, 1395971878, 16777343, 1, '', 0, 0, 100, 100, 100, 10, 100, 1, 0, 0, 0, ''),
(7, 'Third', 123456, 1, 0, 1, 14316087, 97, 0, 85, 85, 10, 10, 18, 0, 2, 69, 95, 78, 97, 136, 32369, 32241, 7, 470, 1396045859, 1396045432, 16777343, 1, '', 4, 1395971878, 100, 100, 100, 10, 100, 1, 0, 0, 0, '');

--
-- Triggers `players`
--
DROP TRIGGER IF EXISTS `oncreate_players`;
DELIMITER //
CREATE TRIGGER `oncreate_players` AFTER INSERT ON `players`
 FOR EACH ROW BEGIN
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 0, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 1, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 2, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 3, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 4, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 5, 10);
	INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 6, 10);
END
//
DELIMITER ;
DROP TRIGGER IF EXISTS `ondelete_players`;
DELIMITER //
CREATE TRIGGER `ondelete_players` BEFORE DELETE ON `players`
 FOR EACH ROW BEGIN
	DELETE FROM `bans` WHERE `type` = 2 AND `value` = OLD.`id`;
	UPDATE `houses` SET `owner` = 0 WHERE `owner` = OLD.`id`;
END
//
DELIMITER ;

-- --------------------------------------------------------

--
-- Table structure for table `player_deaths`
--

CREATE TABLE `player_deaths` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `player_id` int(10) unsigned NOT NULL,
  `date` int(10) unsigned NOT NULL,
  `level` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `player_id` (`player_id`),
  KEY `date` (`date`)
) ENGINE=InnoDB  DEFAULT CHARSET=utf8 AUTO_INCREMENT=15 ;

--
-- Dumping data for table `player_deaths`
--

INSERT INTO `player_deaths` (`id`, `player_id`, `date`, `level`) VALUES
(2, 5, 1395905164, 1),
(3, 5, 1395907395, 1),
(6, 5, 1395959287, 1),
(7, 6, 1395971072, 1),
(8, 6, 1395971409, 1),
(9, 6, 1395971878, 1),
(10, 5, 1395971910, 1),
(11, 7, 1395971912, 1),
(12, 7, 1395971961, 1),
(13, 7, 1395972261, 100),
(14, 7, 1395988425, 98);

-- --------------------------------------------------------

--
-- Table structure for table `player_depotitems`
--

CREATE TABLE `player_depotitems` (
  `player_id` int(10) unsigned NOT NULL,
  `sid` int(11) NOT NULL COMMENT 'any given range eg 0-100 will be reserved for depot lockers and all > 100 will be then normal items inside depots',
  `pid` int(11) NOT NULL DEFAULT '0',
  `itemtype` int(11) NOT NULL,
  `count` int(11) NOT NULL DEFAULT '0',
  `attributes` blob NOT NULL,
  UNIQUE KEY `player_id` (`player_id`,`sid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Dumping data for table `player_depotitems`
--

INSERT INTO `player_depotitems` (`player_id`, `sid`, `pid`, `itemtype`, `count`, `attributes`) VALUES
(3, 101, 0, 2589, 1, ''),
(3, 102, 101, 2594, 1, ''),
(3, 103, 102, 2148, 40, 0x0f28);

-- --------------------------------------------------------

--
-- Table structure for table `player_items`
--

CREATE TABLE `player_items` (
  `player_id` int(10) unsigned NOT NULL,
  `sid` int(11) NOT NULL,
  `pid` int(11) NOT NULL DEFAULT '0',
  `itemtype` int(11) NOT NULL,
  `count` int(11) NOT NULL DEFAULT '0',
  `attributes` blob COMMENT 'replaces unique_id, action_id, text, special_desc',
  UNIQUE KEY `player_id` (`player_id`,`sid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Dumping data for table `player_items`
--

INSERT INTO `player_items` (`player_id`, `sid`, `pid`, `itemtype`, `count`, `attributes`) VALUES
(3, 101, 5, 2152, 98, 0x0f62),
(3, 102, 6, 2183, 1, ''),
(3, 103, 8, 2643, 1, ''),
(5, 101, 1, 2480, 1, ''),
(5, 102, 3, 1987, 1, ''),
(5, 103, 8, 2195, 1, ''),
(7, 101, 3, 1987, 1, '');

-- --------------------------------------------------------

--
-- Table structure for table `player_killers`
--

CREATE TABLE `player_killers` (
  `kill_id` int(10) unsigned NOT NULL,
  `player_id` int(10) unsigned NOT NULL,
  `unjustified` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`kill_id`,`player_id`),
  KEY `player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Dumping data for table `player_killers`
--

INSERT INTO `player_killers` (`kill_id`, `player_id`, `unjustified`) VALUES
(7, 5, 1),
(8, 7, 1),
(9, 3, 0),
(10, 7, 1),
(11, 5, 1),
(12, 7, 1),
(13, 3, 0),
(14, 3, 0),
(15, 6, 0),
(16, 3, 0),
(17, 5, 0);

-- --------------------------------------------------------

--
-- Table structure for table `player_skills`
--

CREATE TABLE `player_skills` (
  `player_id` int(10) unsigned NOT NULL,
  `skillid` int(10) unsigned NOT NULL,
  `value` int(10) unsigned NOT NULL DEFAULT '0',
  `count` int(10) unsigned NOT NULL DEFAULT '0',
  KEY `player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Dumping data for table `player_skills`
--

INSERT INTO `player_skills` (`player_id`, `skillid`, `value`, `count`) VALUES
(3, 0, 10, 0),
(3, 1, 10, 0),
(3, 2, 10, 0),
(3, 3, 10, 0),
(3, 4, 10, 0),
(3, 5, 10, 0),
(3, 6, 10, 0),
(5, 0, 11, 21),
(5, 1, 10, 0),
(5, 2, 10, 0),
(5, 3, 10, 0),
(5, 4, 10, 0),
(5, 5, 10, 0),
(5, 6, 10, 0),
(6, 0, 10, 0),
(6, 1, 10, 0),
(6, 2, 10, 0),
(6, 3, 10, 0),
(6, 4, 10, 0),
(6, 5, 10, 0),
(6, 6, 10, 0),
(7, 0, 10, 37),
(7, 1, 10, 0),
(7, 2, 10, 0),
(7, 3, 10, 0),
(7, 4, 10, 0),
(7, 5, 10, 0),
(7, 6, 10, 0);

-- --------------------------------------------------------

--
-- Table structure for table `player_spells`
--

CREATE TABLE `player_spells` (
  `player_id` int(10) unsigned NOT NULL,
  `name` varchar(255) NOT NULL,
  KEY `player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `player_storage`
--

CREATE TABLE `player_storage` (
  `player_id` int(10) unsigned NOT NULL,
  `key` int(11) NOT NULL,
  `value` int(11) NOT NULL,
  KEY `player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Dumping data for table `player_storage`
--

INSERT INTO `player_storage` (`player_id`, `key`, `value`) VALUES
(6, 25000, 1),
(6, 1000000, 1),
(6, 2000000, 1),
(5, 25000, 1),
(5, 1000000, -1),
(5, 2000000, -1),
(7, 25000, 1),
(7, 1000000, -1),
(7, 2000000, -1);

-- --------------------------------------------------------

--
-- Table structure for table `player_viplist`
--

CREATE TABLE `player_viplist` (
  `player_id` int(10) unsigned NOT NULL COMMENT 'id of player whose viplist entry it is',
  `vip_id` int(10) unsigned NOT NULL COMMENT 'id of target player of viplist entry',
  KEY `player_id` (`player_id`),
  KEY `vip_id` (`vip_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `schema_info`
--

CREATE TABLE `schema_info` (
  `name` varchar(255) NOT NULL,
  `value` varchar(255) NOT NULL,
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Dumping data for table `schema_info`
--

INSERT INTO `schema_info` (`name`, `value`) VALUES
('version', '24');

-- --------------------------------------------------------

--
-- Table structure for table `tiles`
--

CREATE TABLE `tiles` (
  `id` int(10) unsigned NOT NULL,
  `house_id` int(10) unsigned NOT NULL DEFAULT '0',
  `x` int(5) unsigned NOT NULL,
  `y` int(5) unsigned NOT NULL,
  `z` int(2) unsigned NOT NULL,
  PRIMARY KEY (`id`),
  KEY `x` (`x`,`y`,`z`),
  KEY `house_id` (`house_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `tile_items`
--

CREATE TABLE `tile_items` (
  `tile_id` int(10) unsigned NOT NULL,
  `sid` int(11) NOT NULL,
  `pid` int(11) NOT NULL DEFAULT '0',
  `itemtype` int(11) NOT NULL,
  `count` int(11) NOT NULL DEFAULT '0',
  `attributes` blob NOT NULL,
  KEY `sid` (`sid`),
  KEY `tile_id` (`tile_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Constraints for dumped tables
--

--
-- Constraints for table `bans`
--
ALTER TABLE `bans`
  ADD CONSTRAINT `bans_ibfk_1` FOREIGN KEY (`admin_id`) REFERENCES `players` (`id`) ON DELETE SET NULL;

--
-- Constraints for table `environment_killers`
--
ALTER TABLE `environment_killers`
  ADD CONSTRAINT `environment_killers_ibfk_1` FOREIGN KEY (`kill_id`) REFERENCES `killers` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `guilds`
--
ALTER TABLE `guilds`
  ADD CONSTRAINT `guilds_ibfk_1` FOREIGN KEY (`owner_id`) REFERENCES `players` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `guild_invites`
--
ALTER TABLE `guild_invites`
  ADD CONSTRAINT `guild_invites_ibfk_1` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE,
  ADD CONSTRAINT `guild_invites_ibfk_2` FOREIGN KEY (`guild_id`) REFERENCES `guilds` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `guild_members`
--
ALTER TABLE `guild_members`
  ADD CONSTRAINT `guild_members_ibfk_1` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE,
  ADD CONSTRAINT `guild_members_ibfk_2` FOREIGN KEY (`rank_id`) REFERENCES `guild_ranks` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `guild_ranks`
--
ALTER TABLE `guild_ranks`
  ADD CONSTRAINT `guild_ranks_ibfk_1` FOREIGN KEY (`guild_id`) REFERENCES `guilds` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `house_auctions`
--
ALTER TABLE `house_auctions`
  ADD CONSTRAINT `house_auctions_ibfk_1` FOREIGN KEY (`house_id`) REFERENCES `houses` (`id`) ON DELETE CASCADE,
  ADD CONSTRAINT `house_auctions_ibfk_2` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `house_lists`
--
ALTER TABLE `house_lists`
  ADD CONSTRAINT `house_lists_ibfk_1` FOREIGN KEY (`house_id`) REFERENCES `houses` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `killers`
--
ALTER TABLE `killers`
  ADD CONSTRAINT `killers_ibfk_1` FOREIGN KEY (`death_id`) REFERENCES `player_deaths` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `players`
--
ALTER TABLE `players`
  ADD CONSTRAINT `players_ibfk_1` FOREIGN KEY (`account_id`) REFERENCES `accounts` (`id`) ON DELETE CASCADE,
  ADD CONSTRAINT `players_ibfk_2` FOREIGN KEY (`group_id`) REFERENCES `groups` (`id`);

--
-- Constraints for table `player_deaths`
--
ALTER TABLE `player_deaths`
  ADD CONSTRAINT `player_deaths_ibfk_1` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `player_depotitems`
--
ALTER TABLE `player_depotitems`
  ADD CONSTRAINT `player_depotitems_ibfk_1` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `player_items`
--
ALTER TABLE `player_items`
  ADD CONSTRAINT `player_items_ibfk_1` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `player_killers`
--
ALTER TABLE `player_killers`
  ADD CONSTRAINT `player_killers_ibfk_1` FOREIGN KEY (`kill_id`) REFERENCES `killers` (`id`) ON DELETE CASCADE,
  ADD CONSTRAINT `player_killers_ibfk_2` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `player_skills`
--
ALTER TABLE `player_skills`
  ADD CONSTRAINT `player_skills_ibfk_1` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `player_spells`
--
ALTER TABLE `player_spells`
  ADD CONSTRAINT `player_spells_ibfk_1` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `player_storage`
--
ALTER TABLE `player_storage`
  ADD CONSTRAINT `player_storage_ibfk_1` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `player_viplist`
--
ALTER TABLE `player_viplist`
  ADD CONSTRAINT `player_viplist_ibfk_1` FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE,
  ADD CONSTRAINT `player_viplist_ibfk_2` FOREIGN KEY (`vip_id`) REFERENCES `players` (`id`) ON DELETE CASCADE;

--
-- Constraints for table `tiles`
--
ALTER TABLE `tiles`
  ADD CONSTRAINT `tiles_ibfk_1` FOREIGN KEY (`house_id`) REFERENCES `houses` (`id`) ON DELETE NO ACTION;

--
-- Constraints for table `tile_items`
--
ALTER TABLE `tile_items`
  ADD CONSTRAINT `tile_items_ibfk_1` FOREIGN KEY (`tile_id`) REFERENCES `tiles` (`id`) ON DELETE CASCADE;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
