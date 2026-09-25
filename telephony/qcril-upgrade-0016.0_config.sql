/*
  SPDX-License-Identifier: Apache-2.0
  Copyright 2026 The DiamaneOS Project

  Turn QCRIL power-up optimisation off in an existing version 15.0 database,
  matching the version 16.0 database in /vendor.
*/

CREATE TABLE IF NOT EXISTS qcril_properties_table (property TEXT PRIMARY KEY NOT NULL, def_val TEXT, value TEXT);
INSERT OR REPLACE INTO qcril_properties_table(property, def_val) VALUES('qcrildb_version',16.0);
UPDATE qcril_properties_table SET def_val="0" WHERE property="persist.vendor.radio.poweron_opt";
