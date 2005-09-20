-- Magick IRC Services
--
-- (c) 2005 The Neuromancy Society <info@neuromancy.net>
--
-- The above copywright may not be removed under any circumstances,
-- however it may be added to if any modifications are made to this
-- file.  All modified code must be clearly documented and labelled.
--
-- This code is released under the GNU General Public License v2.0 or better.
-- The full text of this license should be contained in a file called
-- COPYING distributed with this code.  If this file does not exist,
-- it may be viewed here: http://www.neuromancy.net/license/gpl.html
--
-- =======================================================================
-- $Id$
-- =======================================================================
--
-- For official changes (by The Neuromancy Society), please
-- check the ChangeLog* files that come with this distribution.
--
-- Third Party Changes (please include e-mail address):
--
-- N/A
--
-- =======================================================================

-- This file will create the schema for Magick III in a PostgreSQL database.
-- Although this file MAY work with other databases, it is designed with
-- PostgreSQL in mind, so please do not bother to report a bug if you are
-- using this with a different database.

-- PostgreSQL supports many SQL features that are taken advantage of:
--   - Foreign Keys
--   - Cascading Deletes
--   - Robust Indexing

DROP TABLE killchans;
DROP TABLE ignores;
DROP TABLE operdenies;
DROP TABLE clones;
DROP TABLE akills;
DROP TABLE forbidden;
DROP TABLE committees_member;
DROP TABLE committees_message;
DROP TABLE committees CASCADE;
DROP TABLE channels_level;
DROP TABLE channels_access;
DROP TABLE channels_akick;
DROP TABLE channels_greet;
DROP TABLE channels_message;
DROP TABLE channels_news_read;
DROP TABLE channels_news;
DROP TABLE channels CASCADE;
DROP TABLE nicks;
DROP TABLE users_access;
DROP TABLE users_ignore;
DROP TABLE users_memo;
DROP TABLE users CASCADE;

CREATE TABLE users (
		id integer PRIMARY KEY,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		password varchar(64) NOT NULL,
		email varchar(320), -- Max length of an email (according to RFC)
		website varchar(2048), -- Max length of a URL that IE will support
		icq integer,
		aim varchar(16), -- AOL is not registering SN's longer than 16 chars NOT NULL,
		msn varchar(320), -- Max length of an email (according to RFC) NOT NULL,
		jabber varchar(512), -- Username and domain can both be 255 chars NOT NULL,
		yahoo varchar(32), -- Maximum size yahoo allows you to register NOT NULL,
		description text,
		comment text,
		suspend_by varchar(32),
		suspend_by_id integer REFERENCES users(id) ON DELETE SET NULL,
		suspend_reason text,
		suspend_time timestamp,
		language varchar(16),
		protect boolean,
		secure boolean,
		nomemo boolean,
		private boolean,
		privmsg boolean,
		noexpire boolean,
		picture integer,
		picture_ext varchar(8),
		lock_language boolean NOT NULL DEFAULT false,
		lock_protect boolean NOT NULL DEFAULT false,
		lock_secure boolean NOT NULL DEFAULT false,
		lock_nomemo boolean NOT NULL DEFAULT false,
		lock_private boolean NOT NULL DEFAULT false,
		lock_privmsg boolean NOT NULL DEFAULT false
	);
CREATE INDEX users_suspend_by_id_idx ON users(suspend_by_id);

CREATE TABLE users_access (
		id integer REFERENCES users(id) ON DELETE CASCADE NOT NULL,
		number integer NOT NULL,
		mask varchar(320) NOT NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (id, number)
	);

CREATE TABLE users_ignore (
		id integer REFERENCES users(id) ON DELETE CASCADE NOT NULL,
		number integer NOT NULL,
		entry integer REFERENCES users(id) ON DELETE CASCADE NOT NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (id, number),
		UNIQUE (id, entry)
	);

CREATE INDEX users_ignore_entry_idx ON users_ignore(entry);

-- The 'sender' just references who the user was signed in as when
-- they sent the memo, but it may not be current (it could have been
-- dropped, etc).  The sender_id should be used for finding the origin
-- of the message, however it may be NULL if the origin drops ALL their
-- nicknames (not hard if they only have one).
CREATE TABLE users_memo (
		id integer REFERENCES users(id) ON DELETE CASCADE NOT NULL,
		number integer NOT NULL,
		sent timestamp NOT NULL DEFAULT current_timestamp,
		sender varchar(32) NOT NULL, -- May not be current, but has to be there.
		sender_id integer REFERENCES users(id) ON DELETE SET NULL, -- The REAL thing
		message text NOT NULL,
		read boolean,
		attachment integer,
		PRIMARY KEY (id, number)
	);
CREATE INDEX users_memo_sender_id_idx ON users_memo(sender_id);

CREATE TABLE nicks (
		name varchar(32) PRIMARY KEY, -- RFC2812 allows 9, but some go higher
		id integer REFERENCES users(id) ON DELETE CASCADE NOT NULL,
		registered timestamp NOT NULL DEFAULT current_timestamp,
		last_realname varchar(50),
		last_mask varchar(320),
		last_quit varchar(300),
		last_seen timestamp);
CREATE UNIQUE INDEX nicks_idx ON nicks(lower(name));
CREATE INDEX nicks_id_idx ON nicks(id);

CREATE TABLE committees (
		id integer PRIMARY KEY,
		name varchar(32) NOT NULL,
		head_user integer REFERENCES users(id) ON DELETE CASCADE,
		head_committee integer REFERENCES committees(id) ON DELETE CASCADE,
		registered timestamp NOT NULL DEFAULT current_timestamp,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		description text,
		email varchar(320),
		webpage varchar(2048),
		private boolean,
		openmemos boolean,
		secure boolean,
		lock_private boolean NOT NULL DEFAULT false,
		lock_openmemos boolean NOT NULL DEFAULT false,
		lock_secure boolean NOT NULL DEFAULT false,
		comment text
	);
CREATE UNIQUE INDEX committees_name_idx ON committees(lower(name));
CREATE INDEX committees_head_user_idx ON committees(head_user);
CREATE INDEX committees_head_committee_idx ON committees(head_committee);

CREATE TABLE committees_member (
		id integer REFERENCES committees(id) ON DELETE CASCADE NOT NULL,
		number integer NOT NULL,
		entry integer REFERENCES users(id) ON DELETE CASCADE NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (id, number), UNIQUE(id, entry)
	);
CREATE INDEX committees_member_id_idx ON committees_member(id);
CREATE INDEX committees_member_entry_idx ON committees_member(entry);
CREATE INDEX committees_member_last_updater_id_idx ON committees_member(last_updater_id);

CREATE TABLE committees_message (
		id integer REFERENCES committees(id) ON DELETE CASCADE NOT NULL,
		number integer NOT NULL,
		message text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY(id, number)
	);
CREATE INDEX committees_message_id_idx ON committees_message(id);
CREATE INDEX committees_message_last_updater_id_idx ON committees_message(last_updater_id);

CREATE TABLE channels (
		id integer PRIMARY KEY,
		name varchar(64), -- RFC2812 allows up to 50
		registered timestamp NOT NULL DEFAULT current_timestamp,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		last_used timestamp,
		password varchar(64) NOT NULL,
		-- We restrict the founder, since successor has to be taken into account.
		founder integer REFERENCES users(id) ON DELETE RESTRICT NOT NULL,
		successor integer REFERENCES users(id) ON DELETE SET NULL,
		description text,
		email varchar(320),
		website varchar(2048),
		comment text,
		topic varchar(300),
		topic_setter varchar(32),
		topic_set_time timestamp,
		keeptopic boolean,
		topiclock boolean,
		private boolean,
		secureops boolean,
		secure boolean,
		anarchy boolean,
		kickonban boolean,
		restricted boolean,
		cjoin boolean,
		noexpire boolean,
		bantime interval,
		parttime interval,
		revenge "char",
		mlock_on varchar(32),
		mlock_off varchar(32),
		mlock_key varchar(32),
		mlock_limit integer,
		lock_keeptopic boolean NOT NULL DEFAULT false,
		lock_topiclock boolean NOT NULL DEFAULT false,
		lock_private boolean NOT NULL DEFAULT false,
		lock_secureops boolean NOT NULL DEFAULT false,
		lock_secure boolean NOT NULL DEFAULT false,
		lock_anarchy boolean NOT NULL DEFAULT false,
		lock_kickonban boolean NOT NULL DEFAULT false,
		lock_restricted boolean NOT NULL DEFAULT false,
		lock_cjoin boolean NOT NULL DEFAULT false,
		lock_bantime boolean NOT NULL DEFAULT false,
		lock_parttime boolean NOT NULL DEFAULT false,
		lock_revenge boolean NOT NULL DEFAULT false,
		lock_mlock_on varchar(32),
		lock_mlock_off varchar(32),
		suspend_by varchar(32),
		suspend_by_id integer REFERENCES users(id) ON DELETE SET NULL,
		suspend_reason text,
		suspend_time timestamp
	);
CREATE UNIQUE INDEX channels_name_idx ON channels(lower(name));
CREATE INDEX channels_suspend_by_id_idx ON channels(suspend_by_id);
CREATE INDEX channels_founder_idx ON channels(founder);
CREATE INDEX channels_successor_idx ON channels(successor);

CREATE TABLE channels_level (
		id integer REFERENCES channels(id) ON DELETE CASCADE NOT NULL,
		level integer NOT NULL,
		value integer NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (id, level)
	);
CREATE INDEX channels_level_id_idx ON channels_level(id);
CREATE INDEX channels_level_last_updater_id_idx ON channels_level(last_updater_id);

CREATE TABLE channels_access (
		id integer REFERENCES channels(id) ON DELETE CASCADE NOT NULL,
		number integer NOT NULL,
		entry_mask varchar(350),
		entry_user integer REFERENCES users(id) ON DELETE CASCADE,
		entry_committee integer REFERENCES committees(id) ON DELETE CASCADE,
		level integer NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (id, number)
		-- UNIQUE (id, lower(entry_mask), entry_user, entry_committee)
	);
CREATE INDEX channels_access_id_idx ON channels_access(id);
-- CREATE INDEX channels_access_entry_mask_idx ON channels_access(entry_mask);
CREATE INDEX channels_access_entry_user_idx ON channels_access(entry_user);
CREATE INDEX channels_access_entry_committee_idx ON channels_access(entry_committee);
CREATE INDEX channels_access_last_updater_id_idx ON channels_access(last_updater_id);

CREATE TABLE channels_akick (
		id integer REFERENCES channels(id) ON DELETE CASCADE NOT NULL,
		number integer NOT NULL,
		entry_mask varchar(350),
		entry_user integer REFERENCES users(id) ON DELETE CASCADE,
		entry_committee integer REFERENCES committees(id) ON DELETE CASCADE,
		reason text NOT NULL,
		creation timestamp NOT NULL DEFAULT current_timestamp,
		length interval,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (id, number)
		-- UNIQUE (id, lower(entry_mask), entry_user, entry_committee)
	);
CREATE INDEX channels_akick_id_idx ON channels_akick(id);
-- CREATE INDEX channels_akick_entry_mask_idx ON channels_akick(entry_mask);
CREATE INDEX channels_akick_entry_user_idx ON channels_akick(entry_user);
CREATE INDEX channels_akick_entry_committee_idx ON channels_akick(entry_committee);
CREATE INDEX channels_akick_last_updater_id_idx ON channels_akick(last_updater_id);

CREATE TABLE channels_greet (
		id integer REFERENCES channels(id) ON DELETE CASCADE NOT NULL,
		entry integer REFERENCES users(id) ON DELETE CASCADE NOT NULL,
		greeting text NOT NULL,
		locked boolean,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (id, entry)
	);
CREATE INDEX channels_greet_id_idx ON channels_greet(id);
CREATE INDEX channels_greet_entry_idx ON channels_greet(entry);
CREATE INDEX channels_greet_last_updater_id_idx ON channels_greet(last_updater_id);

CREATE TABLE channels_message (
		id integer REFERENCES channels(id) ON DELETE CASCADE NOT NULL,
		number integer NOT NULL,
		message text,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (id, number)
	);
CREATE INDEX channels_message_id_idx ON channels_message(id);
CREATE INDEX channels_message_last_updater_id_idx ON channels_message(last_updater_id);

CREATE TABLE channels_news (
		id integer REFERENCES channels(id) ON DELETE CASCADE NOT NULL,
		number integer NOT NULL,
		sender varchar(32) NOT NULL,
		sender_id integer REFERENCES users(id) ON DELETE SET NULL,
		sent timestamp NOT NULL DEFAULT current_timestamp,
		message text NOT NULL,
		PRIMARY KEY (id, number)
	);
CREATE INDEX channels_news_id_idx ON channels_news(id);
CREATE INDEX channels_news_sender_id_idx ON channels_news(sender_id);

CREATE TABLE channels_news_read (
		id integer NOT NULL,
		number integer NOT NULL,
		entry integer NOT NULL REFERENCES users(id) ON DELETE CASCADE,
		PRIMARY KEY (id, number),
		FOREIGN KEY (id, number) REFERENCES channels_news(id, number) ON DELETE CASCADE,
		UNIQUE (id, entry)
	);
CREATE INDEX channels_news_read_id_idx ON channels_news_read(id);
CREATE INDEX channels_news_read_entry_idx ON channels_news_read(entry);

CREATE TABLE forbidden (
		number integer PRIMARY KEY,
		mask varchar(64) NOT NULL,
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);
-- CREATE UNIQUE INDEX forbidden_mask_idx ON forbidden(lower(mask));
CREATE INDEX forbidden_last_updater_id_idx ON forbidden(last_updater_id);

CREATE TABLE akills (
		number integer PRIMARY KEY,
		length interval NOT NULL,
		mask varchar(320) NOT NULL,
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);
-- CREATE UNIQUE INDEX akills_mask_idx ON akills(lower(mask));
CREATE INDEX akills_last_updater_id_idx ON akills(last_updater_id);

CREATE TABLE clones (
		number integer PRIMARY KEY,
		value integer NOT NULL,
		mask varchar(350) NOT NULL,
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);
-- CREATE UNIQUE INDEX clones_mask_idx ON clones(lower(mask));
CREATE INDEX clones_last_updater_id_idx ON clones(last_updater_id);

CREATE TABLE operdenies (
		number integer PRIMARY KEY,
		mask varchar(350) NOT NULL,
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);
-- CREATE UNIQUE INDEX operdenies_mask_idx ON operdenies(lower(mask));
CREATE INDEX operdenies_last_updater_id_idx ON operdenies(last_updater_id);

CREATE TABLE ignores (
		number integer PRIMARY KEY,
		mask varchar(350) NOT NULL,
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);
-- CREATE UNIQUE INDEX ignores_mask_idx ON ignores(lower(mask));
CREATE INDEX ignores_last_updater_id_idx ON ignores(last_updater_id);

CREATE TABLE killchans (
		number integer PRIMARY KEY,
		mask varchar(64),
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id) ON DELETE SET NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);
-- CREATE UNIQUE INDEX killchans_mask_idx ON killchans(lower(mask));
CREATE INDEX killchans_last_updater_id_idx ON killchans(last_updater_id);

