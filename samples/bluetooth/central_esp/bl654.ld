/*
 * Copyright (c) 2023 Jamie M.
 *
 * All right reserved. This code is not apache or FOSS/copyleft licensed.
 */

. = 0x440;
_fake_boot_string_start = .;
KEEP(*(SORT_BY_NAME(.fake_boot_string*)))
_fake_boot_string_size = ABSOLUTE(. - _fake_boot_string_start);

/*
. = 0x2008;
_fake_sd_size_start = .;
KEEP(*(SORT_BY_NAME(.fake_sd_size*)))
_fake_sd_size_size = ABSOLUTE(. - _fake_sd_size_start);
*/
