/*******************************************************************************
 This file is part of PRESTo Early Warning System
 Copyright (C) 2009-2015 Luca Elia

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*******************************************************************************/

/*******************************************************************************

	PRESTo version information

	Change Log:

1.0 ----------------------------------------------------------------------------------------------------------------------

* Removed experimental algorithms: on-site alert levels, PDZ, displacement filtering based on SNR.
  These parts will be finalized and implemented in PRESTo Plus.
* Ignore negligible changes of magnitude again, like in pre-0.2.6 versions.

0.2.8 ---------------------------------------------------------------------------------------------------------------------

* PARAM: replaced "rmean_packets" with "waveform_rmean_secs".
  The realtime waveform mean is calculated on this number of previous seconds (instead of: number of packets) and this is done on
  every second of new incoming data, regardless of packet length. This is needed since the packet duration depends on the data-logger,
  signal content and sampling rate so it can be very variable in heterogeneous network.
  This also eliminates the discrepancy between playback (1-second packets) and realtime (variable duration).
* Initial support for the Earthquake Early Warning Display (EEWD): implemented a simple STOMP client to send alarm messages in
  QuakeML format to a remote server (a message broker, e.g. ActiveMQ). The broker.txt file specifies the server hostname and login
  information. The message broker will broadcast earthquake alarms to all the connected EEW Displays, installed at target sites.
  Each Display will show a map similar to that of PRESTo, with a count-down and expected intensity for the target location.
* BUGFIX: crash when loading SAC files on Windows 8.1 64-bit.
* BUGFIX: PGx uncertainty is now correctly calculated as a min,max range (non-symmetrical around the mean value).
  IMPORTANT: this changes the log file format and the UDP alert format, replacing a single PGx uncertainty with a min,max range.
* PARAM: onsite_disp_ratio_threshold (default: 1.5). A threshold used to decide the best filtering of the waveforms 
  when measuring the on-site parameters. There are now two filter bands like in the case of magnitude:
  "low" band for low magnitude events (high frequencies, e.g. 1-25 Hz) and "high" band for high magnitude events (low
  frequencies, e.g. 0.075-3 Hz). The "low" filter is chosen when the Pd measured using it, Pd_low, is much smaller than Pd
  using the "high" filter, Pd_high, i.e. log10(Pd_high/Pd_low) >= threshold.
* PARAM: magnitude_disp_ratio_threshold (default: 1.5). As in the on-site case, use the "low" filtering when computing magnitude
  if log10(Pd_high/Pd_low) >= threshold. Only used if "magnitude_high_threshold" is disabled (< 0).
* PARAM: waveform_min_snr (default: 5). When computing magnitude or on-site alert levels, do not include stations whose
  Signal-To-Noise Ratio (e.g. RMS after the phase arrival / RMS before the P-pick) is below this threshold (0 to disable).
* PARAM: magnitude_outlier_threshold (default: 3.5). Remove magnitude outliers whose score (proportional to the deviation from the median)
  is above this threshold (0 to disable). Useful to reject e.g. very high values from a broken sensor, or very low values from a wrong pick.
* Improved the event declaration algorithm to minimize false events, especially in large networks (many stations, large distances, high transmission lags):
  - picks coincidence (for event declaration) and pick association to an existing event are now fairly independent of the time order
    in which picks are received by the system, to cope with unpredictable delays and packet lengths.
  - picks coincidence and association require the apparent velocities of the picks (measured from the first, earlier pick)
    to fall within reasonable bounds, compatible with the propagation of P-waves.
  - more aggressive removal of old picks and quakes from memory, to cope with higher network densities.
* PARAM: binder_apparent_vel_min, binder_apparent_vel_max. Range of valid apparent velocity (km/s) for a pick associated to an event.
* PARAM: binder_apparent_vel_stations_spacing. Average inter-station distance (km). Within this distance to the first pick of a quake,
  the apparent velocity checks are not enforced (e.g. the apparent velocity can be "infinite" near the epicenter).
* PARAM: binder_apparent_vel_max_distance. Maximum distance (km) from the first pick of an earthquake valid for the association.
* BUGFIX: in lag simulation, improved the randomization (and gaussian distribution). Made it consistent across different Operating Systems.
* The PDZ palette is stretched to match the user supplied Pd threshold (onsite_disp_threshold). Enabled interpolation of Pd measured at stations.
* BUGFIX: excessive logging of PGX values when no UDP alert was sent (i.e. no target IP defined or in simulation mode).
* Added sensor clipping detection, mostly useful to discard saturated velocimeters in epicentral area.
  Added a column in stations.txt to specify the clipping value for each sensor (in counts, e.g. 80% of (2^24)/2 for 24-bit data loggers).
  When the absolute value of the counts for a channel is higher than the clipping value, declare clipping for several seconds
  ("waveform_clipping_secs" parameter). Early warning parameters (Pd, Tauc, alert level, magnitude) are not computed on a channel during clipping.
* PARAM: waveform_clipping_secs (default: 30). For how many seconds a channel is considered clipped after a clipped sample is detected (0 disables clipping detection).
* BUGFIX: possible crash in simulation mode when loading a station with no vertical component SAC.
* Removed the (redundant) mail log pasted into the full log.
* The statistics of data transmission latencies are reset after being written to the log, so they are more useful (only relative to the last latency_log_period_secs).
* BUGFIX: in simulation mode, after resetting, helicorders were desynched/lagging for a while and heartbeats sending was not reset.
* Allow zooming helicorders and the map while paused.
* Only visible targets are now present in the KML animation. They are labeled with the full name, not the grid name.
* Preliminary support for velocimeters. Added a column to stations.txt, to specify for each station whether it is accelerometric (ACC)
  or velocimetric (VEL).
* PARAM: display_heli_min_vel (default: 0.0001). Minimum full-scale velocity (m/s) to use in the auto-scale of on-screen helicorders
  for velocimetric stations (i.e. high values will shrink noise to a flat line).
* More checks to detect invalid SAC files (e.g. non-SAC files with .sac extension, missing header values).
  Produce a fatal error instead of skipping them. Fill-in missing header values when possible.
* BUGFIX: the map was missing when using a network with a single station and no targets.

0.2.7 ---------------------------------------------------------------------------------------------------------------------

* Updated to the latest libraries:
  SDL 2.0, SDL_Image 2.0, SDL_net 2.0, libPNG 1.6, FilterPicker 5.03, libslink 2.4, muParser 2.2.3.
* Switched sound library from FMod 3.x to SDL_mixer 2.0.
* First Linux version.
* Use explicitly thread-safe time.h functions on all platforms (required for Linux).
* Use SDL_GetTicks for simulated time, not clock() (required for Linux).
* Some fixes for thread safety during data acquisition.
* Fix casting error during data acquisition in case of large packet time differences.
* Error out on unknown parameters in rtloc.txt.

0.2.6a (WIP) --------------------------------------------------------------------------------------------------------------

* Skip picking on horizontal components.
* BUGFIX: crash in simulation mode during an event if the vertical component SAC for a station was missing.
  Such stations are not loaded now.

0.2.6 ---------------------------------------------------------------------------------------------------------------------

* In the log, DISP messages now also report Pd in cm, in addition to Pd in counts.
* PARAM: magnitude_max_value. Clip magnitude PDF distribution to this value (default: 8.0). In previous version the clipping was there, but fixed at 8.0.
* Appended the quake ID to the filename of output files in simulation mode, i.e. <SACS_DIR>/<EVENT_NAME>_<ID>.(png|kml|mail.log).
  This way multiple quakes in the same SAC files produce multiple outputs like in real-time mode.
* Do not convert station name from KSTNM SAC header to uppercase.
* Modified ALARM messages: replaced product of on-site Pd ("PdP") with sum of log10(Pd) ("SumLgPd") to avoid underflow issues.
  Same for sum of on-site tauc ("TcP" -> "SumLgTc"). For consistency, renamed sum of on-site Pd and Tc too ("PdS" -> "SumPd", "TcS" -> "SumTc").
  Used -infinity (-1.#INF in the log) when these data are not available, instead of -1 (since the latter is now a valid value for the sum of logarithms).
  Another change: only consider Pd and tauc in the sums when the alert level is defined (i.e. Pv is above a threshold so that tauc is reliable and we are confident
  we are measuring on signal following an arrival).
* BUGFIX: crash when calculating location uncertainty when probabilities are very low, e.g. false event in real-time -> very low pdf values,
  all below pdf_cut in in rtloc.txt -> uniform distribution (all zeros). Now a default location uncertainty (0) is used in such cases.
* When new signal windows are available to RTMag, consider this a magnitude change even if they do not actually change the magnitude or its uncertainty.
  This increases the log verbosity and produces redundant identical points on the magnitude graph, but is needed to correctly count RTMag windows in the timeline shell.
  Also updated the timeline shell to correctly count available picks before a magnitude is available (i.e. use the new LOCATION messages instead of the QUAKE ones in the log).
* Random selection of helicorders when using SAC files and "display_heli_max_num" (the same selection between runs though).
  Used to be the first stations in SAC filename order.
* PARAM: display_map_station_scale.
  Scale factor to adjust the default size of station icons on the map (also targets, quakes etc.). Useful for very dense or very sparse networks. Default is 1.0.
* Included muParser library (v2.2.2) to evaluate at run time formulas provided by the user as strings in the configuration files.
* Changed the way the predicted PGx at targets and sites is calculated. In previous version fixed prediction laws were used
  (Akkar & Bommer for high magnitudes, Small.pm from ShakeMap for small events), with coefficients specified by the user (pga.txt, pgv.txt).
  Now the user supplies his own functions as one-line text strings in those same files:
  the first to calculate the log10 of PGx and the second to calculate the associated error (all in cm/s or cm/s^2).
  The inputs are the earthquake magnitude, epicentral distance and depth, provided as variables named respectively Mag, R_epi and Dep.
  In order to change law according to e.g. magnitude, a C-style expression such as this can be used: (Mag >= 4.0) ? <formula_high> : <formula_low>.
  Note: the new configuration files use the same laws and coefficients as before.
* Preliminary experimental support for generating the Potential Damage Zone (PDZ, i.e a Pd map) in real-time.
  The map is an interpolation of measured Pd at stations, and theoretical Pd where no stations are present.
  The theoretical Pd is calculated using the distance from the location provided by the regional system and the mean of the measured tauc.
  (Since Pd is a proxy for PGV, a PGV/Imm map is also possible.). Below parameter "display_map_pdz_opacity" should be kept 0 unless when testing the PDZ.
* PARAM: display_map_pdz_opacity.
  Controls the opacity of the PDZ shown on the map (0..1, 0 = completely transparent, i.e. disabled, 1 = completely opaque). Default: 0.9.
* When SeedLink packets are not being received from a station (i.e. large feed latency, shown in red), the last measured data latency is now ignored,
  since it's stale, and it's replaced with 0, also shown in red.
* Wait for the screen drawing to be really finished (i.e. replaced glFlush with glFinish) before saving a screenshot or updating the loading bar.
  (might cure sporadic black screenshots in realtime mode, and no loading bar on some computers / OpenGL drivers?).
* Made config.txt parsing more strict (all parameters must be present, no duplicates) and added its contents to the log too.
* Collect and show all config.txt / params.txt errors on exit, not just the first detected one.
* Made it mandatory to have a 4/3 aspect ratio for the window size. By specifying 0 for both "screen_w" and "screen_h", the window size will be
  automatically chosen to fill 90% of the desktop, either horizontally and/or vertically (100% in full screen mode).
* Skip audio card initialization when running without sound, as no audio lib calls are done in that case.
  Also sound is turned off if there are problems initializing the audio card, instead of exiting with a fatal error.
  Useful when there are installation issues with the audio card driver or no audio card at all.

0.2.56RO (IN PROGRESS) ---------------------------------------------------------------------------------------------------------------------

* <!SPECIAL VERSION FOR ROMANIA!>
* PARAM: removed display_real_quake_lon/lat/dep/mag, moved the real quake information to a file in the earthquake directory
         called <earthquake_name>_real.txt, with format: Lat(deg) Lon(deg) Dep(km) Mag.
         The new boolean parameter display_real_quake controls whether the real location is shown on the map.

0.2.55RO (IN PROGRESS) ---------------------------------------------------------------------------------------------------------------------

* <!SPECIAL VERSION FOR ROMANIA!>
* Use hypocentral distance in PGX formulas, instead of epicentral distance (valid for deep earthquakes in Romania)

0.2.54KR (IN PROGRESS) ---------------------------------------------------------------------------------------------------------------------

* BUGFIX: corrupted textures (e.g. map) if their width was not divisible by 4.
* BUGFIX: potential thread safety issue when sending the mail in real-time mode.
* PARAM: display_heli_max_num.
  Maximum number of helicorders to show on-screen (N). Only the first N stations in seedlink.txt or the first N SAC files found
  (sorted by filename) will be displayed. This can be useful when a large number of stations are used, and the screen is too crowded
  with helicorders (and/or they are too tiny) and/or the performance is poor.
  Note that all stations will be used for Early Warning, regardless of this parameter.
* PARAM: display_heli_lag_threshold.
  When |Tnow-Tlast_sample| is above this many seconds a station is declared lagging, i.e. yellow helicorder, ghosted out on the map.
  Note that this also covers stations with timestamps in the future (GPS problems).
* ESC no longer quits (too easy to press by mistake). Shift-ESC does now.

0.2.53KR (IN PROGRESS) ---------------------------------------------------------------------------------------------------------------------

* BUGFIX: correctly trim spaces from the end of station/channel name read from the SAC header.

0.2.52KR (IN PROGRESS) ---------------------------------------------------------------------------------------------------------------------

* NOTE: this version was distributed to KIGAM staff during the meeting in Naples (April 2012).
* Station icons on the map have a minimum size now, in order to be visible even if the location grid is very large.

0.2.51KR (IN PROGRESS) ---------------------------------------------------------------------------------------------------------------------

* BUGFIX: crash if out of memory while reading the grids at startup.
* Enable the allocation of up to 4GB on 64-bit OS, allowing larger grids / more stations to be used (compiled with LARGEADDRESSAWARE linker switch).

0.2.5KR (IN PROGRESS) ---------------------------------------------------------------------------------------------------------------------

* Support for NonLinLoc v6.0 grids (just an additional "FLOAT" appended to the first line of the .hdr files).

0.2.5 ---------------------------------------------------------------------------------------------------------------------

* BUGFIX: error out on duplicate parameters in params.txt.
* BUGFIX: error out on non-integer sampling rates in SAC files.
* BUGFIX: reworked the time handling in the Graphical User Interface: promoted to higher precision (same as that used in the actual Early Warning procedures).
  This should cure any precision degradation in the user facing controls, and might cure a sporadic bug (seen after prolonged running non-working periods?)
  whereby clicking on waveforms/map does not zoom them. Also prevented a possible halt after running for about 49 days.
* BUGFIX: corrupted screenshots if the screen width was not divisible by 4.
* BUGFIX: removed blank border in texture/wave.png. It was causing smaller-than-requested wavefront radiuses on the map.
* BUGFIX: crash when converting to text out-of-range timestamps sporadically sent by the data-logger (now they are silently converted to the Epoch timestamp when printing).
* Throw away SeedLink packets more than a day in the past or the future, or with unrealistic meta-values (0 samples, negative or too high sample rate).
  Note that this is done after libSlink has handled the data, so malformed or wrong packets may still crash PRESTo there.
* When the last samples from a station are more than 10 seconds in the future, the station is shown on screen in the same way as a lagging station:
  yellow helicorder, ghosted icon on the map.
* Each target site now requires the corresponding travel-times grid. The old code enabled the quick addition of arbitrary targets given their coordinates,
  by using search and interpolation algorithms over the available station grids only. But it was inaccurate and would fail around the borders of the grid.
  Also, to compute the wavefront radius we're now using the grid of the station/target closest to ground level. On-map target text is now split in two lines.
* Updated FilterPicker to a newer version (date of last modified file: 2012-02-09).
* Moved filtering after double integration when measuring Pd.
* On-site parameters for low magnitude and high magnitude were collapsed together (implicitly for high magnitude events).
* PARAM: onsite_all_picks.
  If non-zero, on-site alerts will be computed for every pick, regardless of the regional event detection and magnitude estimation.
  If zero only the binder picks will be used (e.g. on-site alerts only after an event has been declared by the regional system)
* PARAM: onsite_vel_threshold.
  Do not provide an on-site alert level if the peak velocity (Pv) in the on-site time window is below this value (regardless of Pd, tauc).
* On-site alert levels are now shown in the KML animation too, if shown on screen (i.e. if display_show_onsite is non-zero)
* Alarms sent to targets (via UDP) now also contain:
  alarm sequence number (target can discard alarms with a lower sequence number than the last received), epicentral distance (km),
  sum and product of all on-site tauc, sum and product of all on-site Pd, number of stations used in the previous sums and products (stations with on-site params).
* PARAM: alarm_max_period.
  Send an alarm at least every this many seconds per quake, even if source parameters, on-site parameters and PGx at target have not changed
  (target has fewer remaining seconds, anyway).
* The network name and "Real-Time" are shown at the top of the window in real time mode.
* Removed explicit grid filenames from rtloc.txt, they're now hardcoded to: data/<network>/time/layer.<wave>.<station>.time.
* Improved Google Maps URLs in the log: use the new "scale" parameter to double the resolution, and provide an additional URL for the whole map.
* The log now also reports a 1-d approximation of the velocity model in the grids, obtained measuring the velocity at each depth for the first station.
* Portability fixes, now it compiles under Cygwin too (g++ 4.5.3).
* Wrote a user manual for deploying and configuring PRESTo.

0.2.4 ---------------------------------------------------------------------------------------------------------------------

* NOTE: this version was distributed during the first Alert-ES meeting in Naples.
* BUGFIX: on the map, P and S wave-fronts were not correct when zooming on smaller areas than the full location grid.
* BUGFIX: picks with exactly the same timestamps at different stations were considered duplicates.
* BUGFIX: screenshots were not saved every "simulation_movie_period" seconds, but at a lower frequency (due to the time taken to produce and save the screenshot file).
* BUGFIX: during an earthquake, the background color fading from cyan to orange had the two colors swapped.
* PARAMS: "display_real_quake_lon", "display_real_quake_lat", "display_real_quake_dep", "display_real_quake_mag".
  if different from -12345 (-1 for mag) they specify the actual location and magnitude of the earthquake (e.g. from bulletin), to be shown on the map alongside
  the estimated source parameters computed by PRESTo.

0.2.3 - 0.1.1 [snip] -----------------------------------------------------------------------------------------

*******************************************************************************/

#include "version.h"

const string APP_NAME			=	"PRESTo";
const string APP_VERSION		=	"1.0";

const string APP_NAME_VERSION	=	APP_NAME + " v" + APP_VERSION;

const string APP_TITLE			=	APP_NAME_VERSION + " - (C) Luca Elia for RISSC-Lab";

const string APP_BLURB			=	APP_TITLE + "\n" +
									APP_NAME + " is free software, covered by the GNU General Public License, and comes with ABSOLUTELY NO WARRANTY.";
