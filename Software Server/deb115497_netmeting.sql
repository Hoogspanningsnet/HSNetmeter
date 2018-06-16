-- phpMyAdmin SQL Dump
-- version 4.0.10.19
-- https://www.phpmyadmin.net
--
-- Host: localhost
-- Generation Time: Jun 16, 2018 at 08:10 PM
-- Server version: 10.0.29-MariaDB-cll-lve
-- PHP Version: 5.6.36

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;

--
-- Database: `deb115497_netmeting`
--

-- --------------------------------------------------------

--
-- Table structure for table `events`
--

CREATE TABLE IF NOT EXISTS `events` (
  `tm` timestamp(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3) COMMENT 'Tijdstip van het event',
  `type` varchar(15) NOT NULL COMMENT 'Type van het Event',
  `waarde` mediumint(9) NOT NULL COMMENT 'De gemeten waarde',
  `user` smallint(6) NOT NULL COMMENT 'Het station wat het event meldt'
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='Bevat de events zoals gemeld door de stations';

-- --------------------------------------------------------

--
-- Table structure for table `freq`
--

CREATE TABLE IF NOT EXISTS `freq` (
  `time` datetime(3) NOT NULL COMMENT 'The time reported by the client',
  `freq` smallint(6) NOT NULL COMMENT 'The actual frequency reported by the client',
  `volt` smallint(6) NOT NULL COMMENT 'stores the measured voltage asreported by the client ',
  `user` mediumint(9) NOT NULL COMMENT 'The userID , see usertable',
  `dbtime` timestamp(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT 'The time the recoird is stored in the database',
  UNIQUE KEY `usertime` (`time`,`user`),
  KEY `time` (`time`),
  KEY `user` (`user`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=COMPRESSED COMMENT='Stores frequency measurements';

-- --------------------------------------------------------

--
-- Table structure for table `user`
--

CREATE TABLE IF NOT EXISTS `user` (
  `ID` mediumint(9) unsigned NOT NULL AUTO_INCREMENT COMMENT 'the userID of this user',
  `StationName` varchar(30) DEFAULT NULL COMMENT 'The name of the station that reports the measurements',
  `FirstName` text NOT NULL COMMENT 'Firstname of the user',
  `LastName` text COMMENT 'Lastname of the user including the tussenvoegsel',
  `Nick` text COMMENT 'Holds the nickname of this user, this is the name that is shown',
  `Encryptionkey` text NOT NULL,
  `Town` text NOT NULL COMMENT 'Holds the location of the user gerbuiker',
  `Country` char(4) NOT NULL,
  `Lat` float DEFAULT NULL,
  `Lon` float DEFAULT NULL,
  `RpiSerial` bigint(8) unsigned NOT NULL,
  `RpiMAC` bigint(8) unsigned DEFAULT NULL,
  `HV-substation` text NOT NULL COMMENT 'Holds the closest HV-Substation',
  PRIMARY KEY (`ID`),
  UNIQUE KEY `StatioNameIdx` (`StationName`)
) ENGINE=InnoDB  DEFAULT CHARSET=utf8 COMMENT='Holds user data' AUTO_INCREMENT=20001 ;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
