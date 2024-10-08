#ifndef KWS_H
#define KWS_H

#define NUM_CATEOGRIES 3

struct prediction_t {
	char label[32];
	float value;
};

struct timing_t {
	int dsp;
	int classification;
};

struct result_t {
	struct prediction_t predictions[NUM_CATEOGRIES];
	struct timing_t timing;
};

int infer(struct result_t *res);

#endif // !KWS_H
