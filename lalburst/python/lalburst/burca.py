# Copyright (C) 2006--2011,2013,2015--2017  Kipp Cannon
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


#
# =============================================================================
#
#                                   Preamble
#
# =============================================================================
#


from __future__ import print_function


from bisect import bisect_left, bisect_right
import itertools
import math
import sys


from glue.ligolw import lsctables
from . import snglcoinc


__author__ = "Kipp Cannon <kipp.cannon@ligo.org>"
from .git_version import date as __date__
from .git_version import version as __version__


#
# =============================================================================
#
#                                 Speed Hacks
#
# =============================================================================
#


class SnglBurst(lsctables.SnglBurst):
	__slots__ = ()
	def __cmp__(self, other):
		# compare self's peak time to the LIGOTimeGPS instance
		# other.  allows bisection searches by GPS time to find
		# ranges of triggers quickly
		return cmp(self.peak, other)


#
# =============================================================================
#
#                          CoincTables Customizations
#
# =============================================================================
#


#
# For use with excess power.
#


ExcessPowerBBCoincDef = lsctables.CoincDef(search = u"excesspower", search_coinc_type = 0, description = u"sngl_burst<-->sngl_burst coincidences")


class ExcessPowerCoincTables(snglcoinc.CoincTables):
	def __init__(self, xmldoc):
		super(ExcessPowerCoincTables, self).__init__(xmldoc)

		# find the multi_burst table or create one if not found
		try:
			self.multibursttable = lsctables.MultiBurstTable.get_table(xmldoc)
		except ValueError:
			self.multibursttable = lsctables.New(lsctables.MultiBurstTable, ("process_id", "duration", "central_freq", "bandwidth", "snr", "confidence", "amplitude", "coinc_event_id"))
			xmldoc.childNodes[0].appendChild(self.multibursttable)

	def make_multi_burst(self, process_id, coinc_event_id, events, offset_vector):
		multiburst = self.multibursttable.RowType(
			process_id = process_id,
			coinc_event_id = coinc_event_id,
			# populate the false alarm rate with none.
			false_alarm_rate = None
		)

		# snr = root sum of ms_snr squares

		multiburst.snr = math.sqrt(sum(event.ms_snr**2.0 for event in events))

		# peak time = ms_snr squared weighted average of peak times
		# (no attempt to account for inter-site delays).
		# LIGOTimeGPS objects don't like being multiplied by
		# things, so the first event's peak time is used as a
		# reference epoch.

		t = events[0].peak + offset_vector[events[0].ifo]
		multiburst.peak = t + sum(event.ms_snr**2.0 * float(event.peak + offset_vector[event.ifo] - t) for event in events) / multiburst.snr**2.0

		# duration = ms_snr squared weighted average of durations

		multiburst.duration = sum(event.ms_snr**2.0 * event.duration for event in events) / multiburst.snr**2.0

		# central_freq = ms_snr squared weighted average of peak
		# frequencies

		multiburst.central_freq = sum(event.ms_snr**2.0 * event.peak_frequency for event in events) / multiburst.snr**2.0

		# bandwidth = ms_snr squared weighted average of bandwidths

		multiburst.bandwidth = sum(event.ms_snr**2.0 * event.bandwidth for event in events) / multiburst.snr**2.0

		# confidence = minimum of confidences

		multiburst.confidence = min(event.confidence for event in events)

		# "amplitude" = h_rss of event with highest confidence

		multiburst.amplitude = max(events, key = lambda event: event.ms_confidence).ms_hrss

		# done

		return multiburst

	def coinc_rows(self, process_id, time_slide_id, events, table_name):
		coinc, coincmaps = super(ExcessPowerCoincTables, self).coinc_rows(process_id, time_slide_id, events, table_name)
		coinc.insts = (event.ifo for event in events)
		return coinc, coincmaps, self.make_multi_burst(process_id, coinc.coinc_event_id, events, self.time_slide_index[time_slide_id])

	def append_coinc(self, coinc, coincmaps, multiburst):
		coinc = super(ExcessPowerCoincTables, self).append_coinc(coinc, coincmaps)
		multiburst.coinc_event_id = coinc.coinc_event_id
		self.multibursttable.append(multiburst)
		return coinc


#
# For use with string cusp
#


StringCuspBBCoincDef = lsctables.CoincDef(search = u"StringCusp", search_coinc_type = 0, description = u"sngl_burst<-->sngl_burst coincidences")


class StringCuspCoincTables(snglcoinc.CoincTables):
	@staticmethod
	def ntuple_comparefunc(events, offset_vector, disallowed = frozenset(("H1", "H2"))):
		# disallow H1,H2 only coincs
		# FIXME:  implement this in the ranking statistic, not like
		# this
		return set(event.ifo for event in events) == disallowed

	def coinc_rows(self, process_id, time_slide_id, events, table_name):
		coinc, coincmaps = super(StringCuspCoincTables, self).coinc_rows(process_id, time_slide_id, events, table_name)
		coinc.insts = (event.ifo for event in events)
		return coinc, coincmaps


#
# =============================================================================
#
#                            Coincidence Generators
#
# =============================================================================
#


#
# For use with excess power coincidence test
#


class ep_coincgen_doubles(snglcoinc.coincgen_doubles):
	class singlesqueue(snglcoinc.coincgen_doubles.singlesqueue):
		@staticmethod
		def event_time(event):
			return event.peak


	class get_coincs(object):
		@staticmethod
		def max_edge_peak_delta(events):
			# the largest difference between any event's peak
			# time and either its start or stop times
			if not events:
				return 0.0
			return max(max(float(event.peak - event.start), float(event.start + event.duration - event.peak)) for event in events)

		@staticmethod
		def comparefunc(a, offseta, b, light_travel_time):
			if abs(a.central_freq - b.central_freq) > (a.bandwidth + b.bandwidth) / 2:
				return True

			astart = a.start + offseta
			bstart = b.start
			if astart > bstart + b.duration + light_travel_time:
				# a starts after the end of b
				return True

			if bstart > astart + a.duration + light_travel_time:
				# b starts after the end of a
				return True

			# time-frequency times intersect
			return False

		def __init__(self, events):
			self.events = events
			# for this instance, replace the method with the
			# pre-computed value
			self.max_edge_peak_delta = self.max_edge_peak_delta(events)

		def __call__(self, event_a, offset_a, light_travel_time, ignored):
			# event_a's peak time
			peak = event_a.peak

			# difference between event_a's peak and start times
			dt = float(peak - event_a.start)

			# largest difference between event_a's peak time
			# and either its start or stop times
			dt = max(dt, event_a.duration - dt)

			# add our own max_edge_peak_delta and the light
			# travel time between the two instruments (when
			# done, if event_a's peak time differs by more than
			# this much from the peak time of an event in this
			# list then it is *impossible* for them to be
			# coincident)
			dt += self.max_edge_peak_delta + light_travel_time

			# apply time shift
			peak += offset_a

			# extract the subset of events from this list that
			# pass coincidence with event_a (use bisection
			# searches for the minimum and maximum allowed peak
			# times to quickly identify a subset of the full
			# list)
			return [event_b for event_b in self.events[bisect_left(self.events, peak - dt) : bisect_right(self.events, peak + dt)] if not self.comparefunc(event_a, offset_a, event_b, light_travel_time)]



#
# For use with string coincidence test
#


class string_coincgen_doubles(ep_coincgen_doubles):
	class get_coincs(object):
		def __init__(self, events):
			self.events = events

		def __call__(self, event_a, offset_a, light_travel_time, threshold):
			peak = event_a.peak + offset_a
			coinc_window = threshold + light_travel_time
			return self.events[bisect_left(self.events, peak - coinc_window) : bisect_right(self.events, peak + coinc_window)]


#
# =============================================================================
#
#                                 Library API
#
# =============================================================================
#


def burca(
	xmldoc,
	process_id,
	coincgen_doubles,
	CoincTables,
	coinc_definer_row,
	threshold,
	delta_t,
	ntuple_comparefunc = lambda events, offset_vector: False,
	min_instruments = 2,
	verbose = False
):
	#
	# prepare the coincidence table interface.
	#

	if verbose:
		print("indexing ...", file=sys.stderr)
	coinc_tables = CoincTables(xmldoc, coinc_definer_row)

	#
	# construct offset vector assembly graph
	#

	time_slide_graph = snglcoinc.TimeSlideGraph(coincgen_doubles, coinc_tables.time_slide_index, delta_t, min_instruments = min_instruments, verbose = verbose)

	#
	# collect events.
	#

	sngl_burst_table = lsctables.SnglBurstTable.get_table(xmldoc)
	for instrument, events in itertools.groupby(sorted(sngl_burst_table, key = lambda row: row.ifo), lambda event: event.ifo):
		events = tuple(events)
		time_slide_graph.push(instrument, events, max(event.peak for event in events))

	#
	# retrieve all coincidences, apply the final n-tuple compare func
	# and record the survivors
	#

	for node, events in time_slide_graph.pull(threshold, flush = True, verbose = verbose):
		if not ntuple_comparefunc(events, node.offset_vector):
			coinc_tables.append_coinc(*coinc_tables.coinc_rows(process_id, node.time_slide_id, events, u"sngl_burst"))

	#
	# done
	#

	return xmldoc
