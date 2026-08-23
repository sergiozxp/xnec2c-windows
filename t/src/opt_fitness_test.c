/*
 *  Unit tests for opt_fitness.c
 *
 *  Tests fitness_transform(), reduce_values (via fitness_compute),
 *  fitness_config dynamic operations, and full fitness_compute()
 *  with synthetic measurement_t arrays.
 *
 *  Copyright (C) 2025 eWheeler, Inc. <https://www.linuxglobal.com/>
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "opt_fitness.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_NEAR(actual, expected, tol, msg) \
	do { \
		tests_run++; \
		double _a = (actual), _e = (expected), _t = (tol); \
		if (fabs(_a - _e) <= _t) { \
			tests_passed++; \
		} \
		else { \
			tests_failed++; \
			fprintf(stderr, "FAIL %s:%d: %s\n" \
				"  expected: %.10g  actual: %.10g  diff: %.10g  tol: %.10g\n", \
				__FILE__, __LINE__, (msg), _e, _a, fabs(_a - _e), _t); \
		} \
	} while (0)

#define ASSERT_TRUE(cond, msg) \
	do { \
		tests_run++; \
		if ((cond)) { \
			tests_passed++; \
		} \
		else { \
			tests_failed++; \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
		} \
	} while (0)

/*------------------------------------------------------------------------*/

/**
 * build_meas - construct synthetic measurement_t array
 * @n: number of steps
 * @field_idx: MEAS_* index to populate
 * @values: array of n values for the field
 * @out: output measurement_t array (caller allocated)
 *
 * Initializes all fields to -1, then sets the specified field.
 */
static void build_meas(int n, int field_idx, const double *values,
	measurement_t *out)
{
	int i;
	int j;

	for (i = 0; i < n; i++)
	{
		for (j = 0; j < MEAS_COUNT; j++)
		{
			out[i].a[j] = -1.0;
		}

		out[i].a[field_idx] = values[i];
	}
}

/*------------------------------------------------------------------------*/

static void test_transform_minimize(void)
{
	double result;
	double norm;
	double tau;

	/* At target: penalty=0, tension at maximum τ
	 * norm = hypot(1.5, 1) = √(3.25), tau = pow(1e-4, 2) = 1e-8 */
	norm = hypot(1.5, 1.0);
	tau = pow(FITNESS_TENSION, 2.0);
	result = fitness_transform(FIT_DIR_MINIMIZE, 1.5, 1.5, 2.0);
	ASSERT_NEAR(result, tau, 1e-12, "MINIMIZE at target");

	/* Above target (VSWR 3.0, target 1.5):
	 * excess = 1.5, score = (1.5/norm)^2 + tau */
	result = fitness_transform(FIT_DIR_MINIMIZE, 3.0, 1.5, 2.0);
	ASSERT_NEAR(result, pow(1.5 / norm, 2.0) + tau, 1e-10,
		"MINIMIZE above target");

	/* Below target (VSWR 1.2, target 1.5): met, tension decreasing
	 * overshoot = 0.3, score = tau / (1 + 0.3/norm) */
	result = fitness_transform(FIT_DIR_MINIMIZE, 1.2, 1.5, 2.0);
	ASSERT_NEAR(result, tau / (1.0 + 0.3 / norm), 1e-12,
		"MINIMIZE below target");

	/* Exponent 1.0: linear excess
	 * excess = 1.5, tau = pow(1e-4, 1) = 1e-4 */
	tau = pow(FITNESS_TENSION, 1.0);
	result = fitness_transform(FIT_DIR_MINIMIZE, 3.0, 1.5, 1.0);
	ASSERT_NEAR(result, 1.5 / norm + tau, 1e-10,
		"MINIMIZE linear exponent");

	/* Negative target: value above target is penalized
	 * norm = hypot(-15, 1), excess = 10 */
	norm = hypot(-15.0, 1.0);
	tau = pow(FITNESS_TENSION, 2.0);
	result = fitness_transform(FIT_DIR_MINIMIZE, -5.0, -15.0, 2.0);
	ASSERT_NEAR(result, pow(10.0 / norm, 2.0) + tau, 1e-10,
		"MINIMIZE negative target, above");

	/* Negative target: value below target is met
	 * overshoot = 5 */
	result = fitness_transform(FIT_DIR_MINIMIZE, -20.0, -15.0, 2.0);
	ASSERT_NEAR(result, tau / (1.0 + 5.0 / norm), 1e-12,
		"MINIMIZE negative target, below");

	/* Cross-sign: positive value, negative target
	 * excess = 20 */
	result = fitness_transform(FIT_DIR_MINIMIZE, 5.0, -15.0, 2.0);
	ASSERT_NEAR(result, pow(20.0 / norm, 2.0) + tau, 1e-10,
		"MINIMIZE cross-sign");
}

/*------------------------------------------------------------------------*/

static void test_transform_maximize(void)
{
	double result;
	double norm;
	double tau;

	/* At target: penalty=0, tension at maximum τ
	 * norm = hypot(8, 1), tau = pow(1e-4, 0.5) */
	norm = hypot(8.0, 1.0);
	tau = pow(FITNESS_TENSION, 0.5);
	result = fitness_transform(FIT_DIR_MAXIMIZE, 8.0, 8.0, 0.5);
	ASSERT_NEAR(result, tau, 1e-10, "MAXIMIZE at target");

	/* Above target (gain 10, target 8): met, overshoot = 2
	 * score = tau / (1 + 2/norm) */
	result = fitness_transform(FIT_DIR_MAXIMIZE, 10.0, 8.0, 0.5);
	ASSERT_NEAR(result, tau / (1.0 + 2.0 / norm), 1e-10,
		"MAXIMIZE above target");

	/* Below target (gain 4, target 8):
	 * shortfall = 4, score = (4/norm)^0.5 + tau */
	result = fitness_transform(FIT_DIR_MAXIMIZE, 4.0, 8.0, 0.5);
	ASSERT_NEAR(result, pow(4.0 / norm, 0.5) + tau, 1e-10,
		"MAXIMIZE below target");

	/* G/T-style: negative target -15, value -10 (above on number line, met)
	 * norm = hypot(-15, 1), overshoot = 5 */
	norm = hypot(-15.0, 1.0);
	tau = pow(FITNESS_TENSION, 2.0);
	result = fitness_transform(FIT_DIR_MAXIMIZE, -10.0, -15.0, 2.0);
	ASSERT_NEAR(result, tau / (1.0 + 5.0 / norm), 1e-12,
		"MAXIMIZE negative target, met");

	/* Negative target -15, value -20 (below on number line, not met)
	 * shortfall = -15 - (-20) = 5 */
	result = fitness_transform(FIT_DIR_MAXIMIZE, -20.0, -15.0, 2.0);
	ASSERT_NEAR(result, pow(5.0 / norm, 2.0) + tau, 1e-10,
		"MAXIMIZE negative target, not met");

	/* Value at zero, positive target:
	 * norm = hypot(8, 1), shortfall = 8, tau for exp=1 */
	norm = hypot(8.0, 1.0);
	tau = pow(FITNESS_TENSION, 1.0);
	result = fitness_transform(FIT_DIR_MAXIMIZE, 0.0, 8.0, 1.0);
	ASSERT_NEAR(result, 8.0 / norm + tau, 1e-10,
		"MAXIMIZE value at zero");

	/* Cross-sign: positive value, negative target (met, overshoot=20)
	 * norm = hypot(-15, 1) */
	norm = hypot(-15.0, 1.0);
	tau = pow(FITNESS_TENSION, 2.0);
	result = fitness_transform(FIT_DIR_MAXIMIZE, 5.0, -15.0, 2.0);
	ASSERT_NEAR(result, tau / (1.0 + 20.0 / norm), 1e-12,
		"MAXIMIZE cross-sign positive value (met)");

	/* Cross-sign: negative value, positive target (not met, shortfall=13)
	 * norm = hypot(8, 1) */
	norm = hypot(8.0, 1.0);
	result = fitness_transform(FIT_DIR_MAXIMIZE, -5.0, 8.0, 2.0);
	ASSERT_NEAR(result, pow(13.0 / norm, 2.0) + tau, 1e-10,
		"MAXIMIZE cross-sign negative value");

	/* G/T with target=5: value=10 (met, overshoot=5)
	 * norm = hypot(5, 1) */
	norm = hypot(5.0, 1.0);
	result = fitness_transform(FIT_DIR_MAXIMIZE, 10.0, 5.0, 2.0);
	ASSERT_NEAR(result, tau / (1.0 + 5.0 / norm), 1e-12,
		"MAXIMIZE G/T met");

	/* G/T with target=5: value=-3 (not met, shortfall=8) */
	result = fitness_transform(FIT_DIR_MAXIMIZE, -3.0, 5.0, 2.0);
	ASSERT_NEAR(result, pow(8.0 / norm, 2.0) + tau, 1e-10,
		"MAXIMIZE G/T not met");

	/* G/T with target=0: value=5 (met, overshoot=5)
	 * norm = hypot(0, 1) = 1.0 — no epsilon guard needed */
	norm = hypot(0.0, 1.0);
	result = fitness_transform(FIT_DIR_MAXIMIZE, 5.0, 0.0, 2.0);
	ASSERT_NEAR(result, tau / (1.0 + 5.0 / norm), 1e-12,
		"MAXIMIZE target zero, met");

	/* G/T with target=0: value=-5 (not met, shortfall=5) */
	result = fitness_transform(FIT_DIR_MAXIMIZE, -5.0, 0.0, 2.0);
	ASSERT_NEAR(result, pow(5.0 / norm, 2.0) + tau, 1e-10,
		"MAXIMIZE target zero, not met");
}

/*------------------------------------------------------------------------*/

static void test_transform_deviate(void)
{
	double result;

	/* At target: score = 0.0 */
	result = fitness_transform(FIT_DIR_DEVIATE, 90.0, 90.0, 2.0);
	ASSERT_NEAR(result, 0.0, 1e-10, "DEVIATE at target");

	/* 5 degrees off: |95-90|^2 = 25 */
	result = fitness_transform(FIT_DIR_DEVIATE, 95.0, 90.0, 2.0);
	ASSERT_NEAR(result, 25.0, 1e-10, "DEVIATE 5 deg positive");

	/* Symmetric: -5 degrees */
	result = fitness_transform(FIT_DIR_DEVIATE, 85.0, 90.0, 2.0);
	ASSERT_NEAR(result, 25.0, 1e-10, "DEVIATE 5 deg negative");

	/* Exponent 1.0: linear deviation */
	result = fitness_transform(FIT_DIR_DEVIATE, 95.0, 90.0, 1.0);
	ASSERT_NEAR(result, 5.0, 1e-10, "DEVIATE linear exponent");
}

/*------------------------------------------------------------------------*/

static void test_config_init(void)
{
	fitness_config_t cfg;

	fitness_config_init(&cfg);

	/* Default: 2 objectives — VSWR, gain_max */
	ASSERT_TRUE(cfg.num_obj == 2, "default config has 2 objectives");

	ASSERT_TRUE(cfg.obj[0].meas_index == MEAS_VSWR, "obj[0] is VSWR");
	ASSERT_TRUE(cfg.obj[0].enabled == 1, "VSWR enabled by default");
	ASSERT_NEAR(cfg.obj[0].target, 1.0, 1e-10, "VSWR default target");
	ASSERT_NEAR(cfg.obj[0].weight, 5.0, 1e-10, "VSWR default weight");
	ASSERT_NEAR(cfg.obj[0].exponent, 2.0, 1e-10, "VSWR default exponent");
	ASSERT_TRUE(cfg.obj[0].direction == FIT_DIR_MINIMIZE, "VSWR direction minimize");
	ASSERT_TRUE(isnan(cfg.obj[0].mhz_min), "mhz_min defaults to NAN");
	ASSERT_TRUE(isnan(cfg.obj[0].mhz_max), "mhz_max defaults to NAN");

	ASSERT_TRUE(cfg.obj[1].meas_index == MEAS_GAIN_MAX, "obj[1] is gain_max");
	ASSERT_TRUE(cfg.obj[1].enabled == 1, "gain_max enabled by default");
	ASSERT_NEAR(cfg.obj[1].target, 12.0, 1e-10, "gain_max default target");
	ASSERT_NEAR(cfg.obj[1].weight, 10.0, 1e-10, "gain_max default weight");
	ASSERT_TRUE(cfg.obj[1].direction == FIT_DIR_MAXIMIZE, "gain_max direction maximize");

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

static void test_config_add_remove(void)
{
	fitness_config_t cfg;
	fitness_objective_t *obj;

	fitness_config_init(&cfg);
	ASSERT_TRUE(cfg.num_obj == 2, "init has 2 objectives");

	/* Add a third objective */
	obj = fitness_config_add(&cfg, MEAS_S11);
	obj->enabled = 1;
	ASSERT_TRUE(cfg.num_obj == 3, "after add: 3 objectives");
	ASSERT_TRUE(cfg.obj[2].meas_index == MEAS_S11, "added s11");

	/* Add duplicate measurement */
	obj = fitness_config_add(&cfg, MEAS_VSWR);
	obj->enabled = 1;
	obj->reduce = FIT_REDUCE_MAX;
	obj->target = 2.0;
	ASSERT_TRUE(cfg.num_obj == 4, "after duplicate add: 4 objectives");
	ASSERT_TRUE(cfg.obj[3].meas_index == MEAS_VSWR, "duplicate VSWR");
	ASSERT_TRUE(cfg.obj[3].reduce == FIT_REDUCE_MAX, "duplicate has max reduce");

	/* Remove middle objective (index 1 = gain_max) */
	fitness_config_remove(&cfg, 1);
	ASSERT_TRUE(cfg.num_obj == 3, "after remove: 3 objectives");
	ASSERT_TRUE(cfg.obj[0].meas_index == MEAS_VSWR, "obj[0] still VSWR");
	ASSERT_TRUE(cfg.obj[1].meas_index == MEAS_S11, "obj[1] shifted to s11");
	ASSERT_TRUE(cfg.obj[2].meas_index == MEAS_VSWR, "obj[2] shifted to duplicate VSWR");

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

static void test_config_copy(void)
{
	fitness_config_t src;
	fitness_config_t dst;

	fitness_config_init(&src);

	fitness_config_copy(&dst, &src);

	ASSERT_TRUE(dst.num_obj == src.num_obj, "copy has same count");
	ASSERT_TRUE(dst.obj != src.obj, "copy has different pointer");
	ASSERT_TRUE(dst.obj[0].meas_index == MEAS_VSWR, "copy obj[0] is VSWR");
	ASSERT_TRUE(dst.obj[1].meas_index == MEAS_GAIN_MAX, "copy obj[1] is gain_max");

	/* Modify src; dst must be independent */
	src.obj[0].target = 999.0;
	ASSERT_NEAR(dst.obj[0].target, 1.0, 1e-10, "copy is independent");

	fitness_config_free(&src);
	fitness_config_free(&dst);
}

/*------------------------------------------------------------------------*/

static void test_compute_vswr_only(void)
{
	fitness_config_t cfg;
	measurement_t meas[4];
	double freq[4] = { 144.0, 145.0, 146.0, 147.0 };
	double vswr_vals[4] = { 1.5, 2.0, 1.8, 1.5 };
	double result;
	double expected;

	fitness_config_init(&cfg);

	/* Disable gain (obj[1]), keep only VSWR (obj[0]) */
	cfg.obj[1].enabled = 0;

	build_meas(4, MEAS_VSWR, vswr_vals, meas);

	result = fitness_compute(&cfg, meas, 4, freq);

	/* avg of fitness_transform(MINIMIZE, val, 1.0, 2.0) for each step:
	 * norm = hypot(1.0, 1.0) = sqrt(2), tau = pow(1e-4, 2) = 1e-8
	 * step 0: val=1.5, excess=0.5, (0.5/sqrt(2))^2 + tau
	 * step 1: val=2.0, excess=1.0, (1.0/sqrt(2))^2 + tau
	 * step 2: val=1.8, excess=0.8, (0.8/sqrt(2))^2 + tau
	 * step 3: val=1.5, excess=0.5, (0.5/sqrt(2))^2 + tau
	 * weight 5.0 * avg
	 */
	{
		double n = hypot(1.0, 1.0);
		double tau = pow(FITNESS_TENSION, 2.0);
		double s0 = pow(0.5 / n, 2.0) + tau;
		double s1 = pow(1.0 / n, 2.0) + tau;
		double s2 = pow(0.8 / n, 2.0) + tau;
		double s3 = pow(0.5 / n, 2.0) + tau;

		expected = 5.0 * (s0 + s1 + s2 + s3) / 4.0;
	}
	ASSERT_NEAR(result, expected, 1e-6, "VSWR-only compute");

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

static void test_compute_gain_only(void)
{
	fitness_config_t cfg;
	measurement_t meas[3];
	double freq[3] = { 144.0, 146.0, 148.0 };
	double gain_vals[3] = { 8.0, 10.0, 6.0 };
	double result;
	double expected;

	fitness_config_init(&cfg);

	/* Disable VSWR (obj[0]), keep only gain (obj[1]) */
	cfg.obj[0].enabled = 0;

	build_meas(3, MEAS_GAIN_MAX, gain_vals, meas);

	result = fitness_compute(&cfg, meas, 3, freq);

	/* avg of fitness_transform(MAXIMIZE, val, 12.0, 0.5):
	 * norm = hypot(12.0, 1.0), tau = pow(1e-4, 0.5)
	 * step 0: val=8,  shortfall=4,  (4/norm)^0.5 + tau
	 * step 1: val=10, shortfall=2,  (2/norm)^0.5 + tau
	 * step 2: val=6,  shortfall=6,  (6/norm)^0.5 + tau
	 * weight 10.0 * avg
	 */
	{
		double n = hypot(12.0, 1.0);
		double tau = pow(FITNESS_TENSION, 0.5);

		expected = 10.0 * (pow(4.0/n, 0.5) + pow(2.0/n, 0.5)
			+ pow(6.0/n, 0.5) + 3.0 * tau) / 3.0;
	}
	ASSERT_NEAR(result, expected, 1e-6, "GAIN-only compute");

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

static void test_compute_combined(void)
{
	fitness_config_t cfg;
	measurement_t meas[2];
	double freq[2] = { 145.0, 146.0 };
	double vswr_vals[2] = { 1.5, 2.0 };
	double gain_vals[2] = { 8.0, 10.0 };
	double result;
	double vswr_contrib;
	double gain_contrib;
	int i;

	fitness_config_init(&cfg);

	/* Build measurements with both fields populated */
	for (i = 0; i < 2; i++)
	{
		int j;

		for (j = 0; j < MEAS_COUNT; j++)
		{
			meas[i].a[j] = -1.0;
		}

		meas[i].a[MEAS_VSWR] = vswr_vals[i];
		meas[i].a[MEAS_GAIN_MAX] = gain_vals[i];
	}

	result = fitness_compute(&cfg, meas, 2, freq);

	{
		double nv = hypot(1.0, 1.0);
		double tv = pow(FITNESS_TENSION, 2.0);
		double ng = hypot(12.0, 1.0);
		double tg = pow(FITNESS_TENSION, 0.5);

		vswr_contrib = 5.0 * (pow(0.5 / nv, 2.0) + tv
			+ pow(1.0 / nv, 2.0) + tv) / 2.0;
		gain_contrib = 10.0 * (pow(4.0 / ng, 0.5) + tg
			+ pow(2.0 / ng, 0.5) + tg) / 2.0;
	}

	ASSERT_NEAR(result, vswr_contrib + gain_contrib, 1e-6, "combined VSWR+gain");

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

static void test_compute_mhz_range(void)
{
	fitness_config_t cfg;
	measurement_t meas[5];
	double freq[5] = { 140.0, 144.0, 146.0, 148.0, 152.0 };
	double vswr_vals[5] = { 5.0, 1.5, 1.8, 1.5, 5.0 };
	double result_all;
	double result_band;

	fitness_config_init(&cfg);
	cfg.obj[1].enabled = 0;

	build_meas(5, MEAS_VSWR, vswr_vals, meas);

	/* All frequencies */
	result_all = fitness_compute(&cfg, meas, 5, freq);

	/* Restrict to 144-148 MHz */
	cfg.obj[0].mhz_min = 144.0;
	cfg.obj[0].mhz_max = 148.0;

	result_band = fitness_compute(&cfg, meas, 5, freq);

	/* Band result excludes the 5.0 VSWR outliers */
	ASSERT_TRUE(result_band < result_all, "MHz range excludes outliers");

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

static void test_compute_invalid_measurements(void)
{
	fitness_config_t cfg;
	measurement_t meas[3];
	double freq[3] = { 144.0, 146.0, 148.0 };
	double result;
	int i;
	int j;

	fitness_config_init(&cfg);
	cfg.obj[1].enabled = 0;

	/* All fields set to -1 (invalid sentinel) */
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < MEAS_COUNT; j++)
		{
			meas[i].a[j] = -1.0;
		}
	}

	result = fitness_compute(&cfg, meas, 3, freq);

	/* No valid steps: result is 0 (no contributions) */
	ASSERT_NEAR(result, 0.0, 1e-10, "all invalid measurements yield 0");

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

static void test_reduce_diff(void)
{
	fitness_config_t cfg;
	measurement_t meas[4];
	double freq[4] = { 144.0, 145.0, 146.0, 147.0 };
	double gain_vals[4] = { 6.0, 8.0, 10.0, 7.0 };
	double result;
	fitness_objective_t *obj;

	fitness_config_init(&cfg);

	/* Disable defaults, add gain_max with diff reduce (gain flatness) */
	cfg.obj[0].enabled = 0;
	cfg.obj[1].enabled = 0;

	obj = fitness_config_add(&cfg, MEAS_GAIN_MAX);
	obj->enabled = 1;
	obj->direction = FIT_DIR_MINIMIZE;
	obj->reduce = FIT_REDUCE_DIFF;
	obj->target = 3.0;
	obj->weight = 1.0;
	obj->exponent = 1.0;

	build_meas(4, MEAS_GAIN_MAX, gain_vals, meas);

	result = fitness_compute(&cfg, meas, 4, freq);

	/* diff reduce: max - min of transformed values
	 * MINIMIZE direction, target=3.0, exp=1.0
	 * norm = hypot(3, 1) = sqrt(10), tau = pow(1e-4, 1) = 1e-4
	 * All values > target, so each has penalty + tau:
	 *   t0 = 3/norm + tau, t1 = 5/norm + tau,
	 *   t2 = 7/norm + tau, t3 = 4/norm + tau
	 * diff = t2 - t0 = (7-3)/norm = 4/norm
	 * weight = 1.0 * diff
	 */
	{
		double n = hypot(3.0, 1.0);
		double expected = 1.0 * (4.0 / n);

		ASSERT_NEAR(result, expected, 1e-6, "diff reduce for gain flatness");
	}

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

static void test_reduce_max(void)
{
	fitness_config_t cfg;
	measurement_t meas[3];
	double freq[3] = { 144.0, 146.0, 148.0 };
	double vswr_vals[3] = { 1.5, 3.0, 1.8 };
	double result;
	double expected;

	fitness_config_init(&cfg);
	cfg.obj[1].enabled = 0;
	cfg.obj[0].reduce = FIT_REDUCE_MAX;

	build_meas(3, MEAS_VSWR, vswr_vals, meas);

	result = fitness_compute(&cfg, meas, 3, freq);

	/* max reduce picks worst VSWR step (3.0):
	 * norm = hypot(1, 1), excess = 2.0, tau = pow(1e-4, 2) */
	{
		double n = hypot(1.0, 1.0);
		double tau = pow(FITNESS_TENSION, 2.0);

		expected = 5.0 * (pow(2.0 / n, 2.0) + tau);
	}

	ASSERT_NEAR(result, expected, 1e-6, "max reduce picks worst VSWR");

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

static void test_defaults_table(void)
{
	int i;

	/* Verify all measurement defaults have sensible values */
	for (i = 0; i < MEAS_COUNT; i++)
	{
		/* Measurements barred from goals carry no goal defaults */
		if (meas_fitness_defaults[i].goal_ineligible)
			continue;

		ASSERT_TRUE(meas_fitness_defaults[i].default_exponent > 0.0,
			"default exponent positive");
		ASSERT_TRUE(meas_fitness_defaults[i].default_reduce >= 0
			&& meas_fitness_defaults[i].default_reduce < FIT_REDUCE_COUNT,
			"default reduce in range");
	}

	/* Verify reduce names table */
	for (i = 0; i < FIT_REDUCE_COUNT; i++)
	{
		ASSERT_TRUE(fitness_reduce_names[i] != NULL,
			"reduce name non-NULL");
	}

	/* meas_names and meas_descriptions are tested via measurements_test */
}

/*------------------------------------------------------------------------*/

static void test_zero_weight_skipped(void)
{
	fitness_config_t cfg;
	measurement_t meas[2];
	double freq[2] = { 145.0, 146.0 };
	double vswr_vals[2] = { 5.0, 5.0 };
	double result;

	fitness_config_init(&cfg);
	cfg.obj[1].enabled = 0;
	cfg.obj[0].weight = 0.0;

	build_meas(2, MEAS_VSWR, vswr_vals, meas);

	result = fitness_compute(&cfg, meas, 2, freq);

	ASSERT_NEAR(result, 0.0, 1e-10, "zero weight contributes nothing");

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

static void test_duplicate_measurement(void)
{
	fitness_config_t cfg;
	measurement_t meas[3];
	double freq[3] = { 144.0, 146.0, 148.0 };
	double vswr_vals[3] = { 1.5, 3.0, 1.8 };
	double result;
	double avg_contrib;
	double max_contrib;
	fitness_objective_t *obj;

	fitness_config_init(&cfg);

	/* Disable gain_max */
	cfg.obj[1].enabled = 0;

	/* Add second VSWR objective with max reduce and different target */
	obj = fitness_config_add(&cfg, MEAS_VSWR);
	obj->enabled = 1;
	obj->reduce = FIT_REDUCE_MAX;
	obj->target = 2.0;
	obj->weight = 8.0;
	obj->exponent = 2.0;

	build_meas(3, MEAS_VSWR, vswr_vals, meas);

	result = fitness_compute(&cfg, meas, 3, freq);

	/* First VSWR: avg reduce, target=1.0, weight=5, exp=2
	 * norm = hypot(1, 1), tau = pow(1e-4, 2) */
	{
		double n1 = hypot(1.0, 1.0);
		double t1 = pow(FITNESS_TENSION, 2.0);
		double n2 = hypot(2.0, 1.0);

		avg_contrib = 5.0 * (pow(0.5 / n1, 2.0) + t1
			+ pow(2.0 / n1, 2.0) + t1
			+ pow(0.8 / n1, 2.0) + t1) / 3.0;

		/* Second VSWR: max reduce, target=2.0, weight=8, exp=2
		 * Only step 1 (3.0) exceeds target: excess=1.0 */
		max_contrib = 8.0 * (pow(1.0 / n2, 2.0) + t1);
	}

	ASSERT_NEAR(result, avg_contrib + max_contrib, 1e-6,
		"duplicate VSWR with different reduce/target");

	fitness_config_free(&cfg);
}

/*------------------------------------------------------------------------*/

int main(void)
{
	printf("opt_fitness_test: running tests\n");

	test_transform_minimize();
	test_transform_maximize();
	test_transform_deviate();
	test_config_init();
	test_config_add_remove();
	test_config_copy();
	test_compute_vswr_only();
	test_compute_gain_only();
	test_compute_combined();
	test_compute_mhz_range();
	test_compute_invalid_measurements();
	test_reduce_diff();
	test_reduce_max();
	test_defaults_table();
	test_zero_weight_skipped();
	test_duplicate_measurement();

	printf("\nopt_fitness_test: %d tests, %d passed, %d failed\n",
		tests_run, tests_passed, tests_failed);

	return tests_failed > 0 ? 1 : 0;
}
