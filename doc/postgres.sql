DROP TABLE killchans;
DROP TABLE ignores;
DROP TABLE operdenies;
DROP TABLE clones;
DROP TABLE akills;
DROP TABLE forbidden;
DROP TABLE committees_message;
DROP TABLE committees_member;
DROP TABLE committees;
DROP TABLE channels_news_read;
DROP TABLE channels_news;
DROP TABLE channels_message;
DROP TABLE channels_greet;
DROP TABLE channels_access;
DROP TABLE channels_level;
DROP TABLE channels;
DROP TABLE nicks;
DROP TABLE users_memo;
DROP TABLE users_ignore;
DROP TABLE users_access;
DROP TABLE users;
DROP SEQUENCE users_id_seq;
CREATE SEQUENCE users_id_seq;

CREATE TABLE users (
		id integer PRIMARY KEY DEFAULT nextval('users_id_seq'),
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		last_online varchar(32),
		password varchar(64) NOT NULL,
		email varchar(320), -- Max length of an email (according to RFC)
		website varchar(2048), -- Max length of a URL that IE will support
		icq integer,
		aim varchar(16), -- AOL is not registering SN's longer than 16 chars NOT NULL
		msn varchar(320), -- Max length of an email (according to RFC) NOT NULL
		jabber varchar(512), -- Username and domain can both be 255 chars NOT NULL
		yahoo varchar(32), -- Maximum size yahoo allows you to register NOT NULL
		description text,
		comment text,
		suspended_by varchar(32),
		suspended_by_id varchar(32) REFERENCES users(id),
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
		lock_language boolean,
		lock_protect boolean,
		lock_secure boolean,
		lock_nomemo boolean,
		lock_private boolean,
		lock_privmsg boolean
	);

CREATE TABLE users_access (
		id integer REFERENCES users(id) NOT NULL,
		number integer NOT NULL,
		mask varchar(320) NOT NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (id, number)
	);

CREATE TABLE users_ignore (
		id integer REFERENCES users(id) NOT NULL,
		number integer NOT NULL,
		entry integer REFERENCES users(id) NOT NULL,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (id, number)
	);

-- The 'sender' just references who the user was signed in as when
-- they sent the memo, but it may not be current (it could have been
-- dropped, etc).  The sender_id should be used for finding the origin
-- of the message, however it may be NULL if the origin drops ALL their
-- nicknames (not hard if they only have one).
CREATE TABLE users_memo (
		id integer REFERENCES users(id) NOT NULL,
		number integer NOT NULL,
		sent timestamp NOT NULL DEFAULT current_timestamp,
		sender varchar(32) NOT NULL, -- May not be current, but has to be there.
		sender_id integer REFERENCES users(id), -- The REAL thing
		message text NOT NULL,
		read boolean,
		attachment integer,
		PRIMARY KEY (id, number)
	);

CREATE TABLE nicks (
		name varchar(32) PRIMARY KEY, -- RFC2812 allows 9, but some go higher
		id integer REFERENCES users(id) NOT NULL,
		registered timestamp NOT NULL DEFAULT current_timestamp,
		last_realname varchar(50),
		last_mask varchar(320),
		last_quit varchar(300),
		last_seen timestamp);

ALTER TABLE users ADD FOREIGN KEY (last_online) REFERENCES nicks(name);

CREATE TABLE committees (
		name varchar(32) PRIMARY KEY,
		head_user integer REFERENCES users(id),
		head_committee varchar(32) REFERENCES committees(name),
		registered timestamp NOT NULL DEFAULT current_timestamp,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		description text,
		email varchar(320),
		webpage varchar(2048),
		private boolean,
		openmemos boolean,
		secure boolean,
		lock_private boolean,
		lock_openmemos boolean,
		lock_secure boolean
	);

CREATE TABLE committees_member (
		name varchar(32) REFERENCES committees(name) NOT NULL,
		entry integer REFERENCES users(id) NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (name, entry)
	);

CREATE TABLE committees_message (
		name varchar(32) REFERENCES committees(name) NOT NULL,
		number integer NOT NULL,
		message text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY(name, number)
	);

CREATE TABLE channels (
		name varchar(64) PRIMARY KEY, -- RFC2812 allows up to 50
		registered timestamp NOT NULL DEFAULT current_timestamp,
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		last_used timestamp,
		password varchar(64) NOT NULL,
		founder integer REFERENCES users(id) NOT NULL,
		successor integer REFERENCES users(id),
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
		lock_keeptopic boolean,
		lock_tpoiclock boolean,
		lock_private boolean,
		lock_secureops boolean,
		lock_secure boolean,
		lock_anarchy boolean,
		lock_kickonban boolean,
		lock_restricted boolean,
		lock_cjoin boolean,
		lock_bantime boolean,
		lock_parttime boolean,
		lock_revenge boolean,
		lock_mlock_on varchar(32),
		lock_mlock_off varchar(32),
		suspended_by varchar(32),
		suspended_by_id varchar(32) REFERENCES users(id),
		suspend_reason text,
		suspend_time timestamp
	);

CREATE TABLE channels_level (
		name varchar(64) REFERENCES channels(name) NOT NULL,
		id integer NOT NULL,
		value integer NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (name, id)
	);

CREATE TABLE channels_access (
		name varchar(64) REFERENCES channels(name) NOT NULL,
		number integer NOT NULL,
		entry_mask varchar(350),
		entry_user integer REFERENCES users(id),
		entry_committee varchar(32) REFERENCES committees(name),
		level integer NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (name, number)
	);

CREATE TABLE channels_akick (
		name varchar(64) REFERENCES channels(name) NOT NULL,
		number integer NOT NULL,
		entry_mask varchar(350),
		entry_user integer REFERENCES users(id),
		entry_committee varchar(32) REFERENCES committees(name),
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (name, number)
	);

CREATE TABLE channels_greet (
		name varchar(64) REFERENCES channels(name) NOT NULL,
		entry integer REFERENCES users(id) NOT NULL,
		greeting text NOT NULL,
		locked boolean,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (name, entry)
	);

CREATE TABLE channels_message (
		name varchar(64) REFERENCES channels(name) NOT NULL,
		number integer NOT NULL,
		message text,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp,
		PRIMARY KEY (name, number)
	);

CREATE TABLE channels_news (
		name varchar(64) REFERENCES channels(name) NOT NULL,
		number integer NOT NULL,
		sender varchar(32) NOT NULL,
		sender_id integer REFERENCES users(id),
		sent timestamp NOT NULL DEFAULT current_timestamp,
		message text NOT NULL,
		PRIMARY KEY (name, number)
	);

CREATE TABLE channels_news_read (
		name varchar(64) NOT NULL,
		number integer NOT NULL,
		entry integer NOT NULL,
		FOREIGN KEY (name, number) REFERENCES channels_news(name, number)
	);

CREATE TABLE forbidden (
		name varchar(64) PRIMARY KEY,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);

CREATE TABLE akills (
		number integer PRIMARY KEY,
		length interval NOT NULL,
		mask varchar(320) NOT NULL,
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);

CREATE TABLE clones (
		number integer PRIMARY KEY,
		value integer NOT NULL,
		mask varchar(350) NOT NULL,
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);

CREATE TABLE operdenies (
		number integer PRIMARY KEY,
		mask varchar(350) NOT NULL,
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);


CREATE TABLE ignores (
		number integer PRIMARY KEY,
		mask varchar(350) NOT NULL,
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);

CREATE TABLE killchans (
		name varchar(64) PRIMARY KEY,
		reason text NOT NULL,
		last_updater varchar(32) NOT NULL,
		last_updater_id integer REFERENCES users(id),
		last_update timestamp NOT NULL DEFAULT current_timestamp
	);


