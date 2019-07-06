/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2009 Martin Nordholts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TESTS_H__
#define __TESTS_H__


Gimp * gimp_init_for_testing             (gboolean use_cpu_accel);
Gimp * gimp_init_for_gui_testing         (gboolean use_cpu_accel,
                                          gboolean show_gui);
void   gimp_test_run_temp_mainloop       (guint32  running_time);
void   gimp_test_run_mainloop_until_idle (void);


#endif /* __TESTS_H__ */
