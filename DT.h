#pragma once

#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <random>
#include <numeric>
#include <ppl.h>

using namespace std;

// c is for class number
template<int c> struct DT
{
	const static int dim = 64;
	bool leaf;
	int label;
	vector<float> w, dist;
	float b;
	DT<c> *left, *right;
	DT(): w(dim), dist(c), left(NULL), right(NULL), label(-1) {}

	// free the space used by all the children nodes
	static void Dispose(DT<c> *dt)
	{
		if (!dt) return;
		if (dt->leaf) { delete dt; return; }
		Dispose(dt->left);
		Dispose(dt->right);
		delete dt;
	}

	static DT<c> *ParseFromStream(istream &ifp)
	{
		if (ifp.eof()) return NULL;

		int leaf;
		ifp.read((char *)&leaf, sizeof(int));
		DT<c> *pdt = new DT<c>();
		if (leaf)
		{
			pdt->leaf = true;
			ifp.read((char *)&pdt->label, sizeof(int));
			ifp.read((char *)&pdt->dist[0], sizeof(float) * c);
			return pdt;
		}

		// build the node
		pdt->leaf = false;
		//ifp.read((char *)&pdt->meanx[0], sizeof(float) * dim);
		//ifp.read((char *)&pdt->maxabs[0], sizeof(float) * dim);
		ifp.read((char *)&pdt->w[0], sizeof(float) * dim);
		ifp.read((char *)&pdt->b, sizeof(float));
		// build the left child
		pdt->left = ParseFromStream(ifp);
		// build the right child
		pdt->right = ParseFromStream(ifp);
		return pdt;
	}

	static DT<c> *ParseFromFile(string fn)
	{
		ifstream ifp(fn, ios::binary);
		if (!ifp) { cerr << " cannot open file " << fn << "."; throw exception(); }
		DT<c> *pdt = ParseFromStream(ifp);
		ifp.close();
		return pdt;
	}

	// Compute the entropy of the labels y
	static float Entropy(const vector<int> &y)
	{
		vector<float> hist(c);
		for (int i = 0; i < y.size(); i++)
			hist[y[i]]++;
		float e = 0;
		for (int i = 0; i < c; i++)
		{
			hist[i] /= y.size();
			if (hist[i])
				e -= hist[i] * log(hist[i]);
		}
		return e;
	}

	static int CountDistribution(const vector<int> &y, vector<float> &dist)
	{
		dist.clear();
		dist.resize(c);
		for (int i = 0; i < y.size(); i++)
			dist[y[i]]++;
		for (int i = 0; i < c; i++)
			dist[i] /= y.size();
		int maxI = 0;
		for (int i = 1; i < c; i++)
			if (dist[i] > dist[maxI])
				maxI = i;
		return maxI;
	}

	static void SplitLabels(const vector<float> &wx, float b, const vector<int> &y, vector<int> &y1, vector<int> &y2)
	{
		y1.clear(); y2.clear();
		for (int i = 0; i < y.size(); i++)
		{
			if (wx[i] >= b)
				y1.push_back(y[i]);
			else
				y2.push_back(y[i]);
		}
	}

	// serialize the decision tree to a stream
	void Serialize(ostream &os) const
	{
		int isLeaf = this->leaf ? 1 : 0;
		os.write((const char *)&isLeaf, sizeof(int));
		if (isLeaf)
		{
			os.write((const char *)&this->label, sizeof(int));
			os.write((const char *)&this->dist[0], sizeof(float) * c);
		}
		else
		{
			//os.write((const char *)&this->meanx[0], sizeof(float) * dim);
			//os.write((const char *)&this->maxabs[0], sizeof(float) * dim);
			os.write((const char *)&this->w[0], sizeof(float) * dim);
			os.write((const char *)&this->b, sizeof(float));
			this->left->Serialize(os);
			this->right->Serialize(os);
		}
	}

	// Train a decision tree from the data, with x as the features with dim dimensions, and y as the labels with c classes.
	// Returns a pointer to the result decision tree.
	static DT<c> *Train(vector<float> x, const vector<int> &y, int level)
	{
		vector<float> mu(dim, 0.0f), sigma(dim, 1.0f);
		return _Train(x, y, mu, sigma, level);    // x was automatically copied and won't be affected when invoking this function.
	}

	// Internal implementation of the training algorithm.
	// Note the features will be modified after training here.
	static DT<c> *_Train(vector<float> &x, const vector<int> &y, vector<float> mu, vector<float> sigma, int level)
	{
		const float minEntropy = 0.1;
		const int minSample = 4;
		const int classifierNum = 300;
		const int bNum = 10;

		bool isLeaf = false;
		int n = x.size() / dim;
		//cerr << Entropy(y) << ' ' << y.size() << endl;
		if (Entropy(y) < minEntropy)
		{
			cerr << 'E';
			isLeaf = true;
		}
		if (n < minSample)
		{
			cerr << 'S';
			isLeaf = true;
		}
		if (level < 0)
		{
			cerr << 'L';
			isLeaf = true;
		}

		if (isLeaf)
		{
			// create a leaf node and return
			DT<c> *leaf = new DT<c>();
			leaf->leaf = true;
			leaf->label = CountDistribution(y, leaf->dist);
			return leaf;
		}

		// not a leaf node
		DT<c> *dt = new DT<c>();
		dt->leaf = false;
		cerr << '.';
		// preprocessing: get the mean and stdvar first
		vector<float> means(dim), means2(dim), stdvar(dim);
		for(int i = 0; i < n; i++)
		{
			for (int j = 0; j < dim; j++)
			{
				float xx = x[i * dim + j];
				means[j] += xx;
				means2[j] += xx * xx;
			}
		}
		for (int j = 0; j < dim; j++)
		{
			means[j] /= n;
			means2[j] /= n;
			stdvar[j] = sqrt(means2[j] - means[j] * means[j]);
		}
		for(int i = 0; i < n; i++)
			for (int j = 0; j < dim; j++)
				if (stdvar[j])
					x[i * dim + j] = (x[i * dim + j] - means[j]) / stdvar[j];
				else
					x[i * dim + j] -= means[j];
		// update mu and sigma
		for (int d = 0; d < dim; d++)
		{
			mu[d] += sigma[d] * means[d];
			sigma[d] *= stdvar[d];
		}

		// generate a classifier set
		struct ClasEval { vector<float> w; float b, e, entropy, balance; int y1, y2; };
		vector<ClasEval> clasEvals(classifierNum);
#ifdef _DEBUG
		for (int i = 0; i < classifierNum; i++)
#else
		concurrency::parallel_for (0, classifierNum, [&](int i)
#endif
		{
			// generate a w
			vector<float> w(dim);
			for (int j = 0; j < dim; j++)
				w[j] = ((float)(rand() % 10000) / 10000 - 0.5); // it \in [-0.5, 0.5]
			// compute w^Tx
			vector<float> wx(n);
			for (int k = 0; k < n; k++)
				for (int j = 0; j < dim; j++)
					wx[k] += w[j] * x[k * dim + j];
			vector<float> wx2 = wx; // get a copy to preserve the original order in wx
			sort(wx2.begin(), wx2.end());
			float step = (float)n / (bNum * 5);
			float bestE = 10000, bestB = 0;    // the smaller the better
			float bestEntropy = 0, bestBalance = 0;
			int bestY1 = 0, bestY2 = 0;
			// generate bs and pick up the best b and record the result
			for (int k = 0; k < bNum; k++)
			{
				float b = wx2[(int)(step * (k + bNum * 2))];
				vector<int> y1, y2;
				SplitLabels(wx, b, y, y1, y2);
				vector<float> dist1, dist2;
				CountDistribution(y1, dist1);
				CountDistribution(y2, dist2);
				float entropy = 0;
				for (int j = 0; j < c; j++)
					entropy += dist1[j] * dist2[j];

#ifdef _DEBUG
				//cerr << endl << e << " <--> " << 0.5f * min(y1.size(), y2.size()) / y.size() << endl;
#endif
				float balance = 0.1f * min(y1.size(), y2.size()) / y.size();
				float e = entropy - balance;
				if (bestE > e)
				{
					bestE = e;
					bestB = b;
					bestY1 = y1.size();
					bestY2 = y2.size();
					bestEntropy = entropy;
					bestBalance = balance;
				}
			}
			clasEvals[i].w = w;
			clasEvals[i].b = -bestB;    // another tricky part
			clasEvals[i].e = bestE;
			clasEvals[i].y1 = bestY1;
			clasEvals[i].y2 = bestY2;
			clasEvals[i].entropy = bestEntropy;
			clasEvals[i].balance = bestBalance;
#ifdef _DEBUG
		}
#else
		});
#endif

		// pick up the best classifier
		int bestJ = 0;
		for (int j = 1; j < classifierNum; j++)
			if (clasEvals[bestJ].e > clasEvals[j].e)
				bestJ = j;
		dt->w = clasEvals[bestJ].w;
		dt->b = clasEvals[bestJ].b;

		// split the training data and train recursively
		vector<float> x1, x2;
		vector<int> y1, y2;
		for (int k = 0; k < n; k++)
		{
			float wx = 0;
			for (int j = 0; j < dim; j++)
				wx += dt->w[j] * x[k * dim + j];
			if (wx + dt->b >= 0)
			{
				for (int j = 0; j < dim; j++)
					x1.push_back(x[k * dim + j]);
				y1.push_back(y[k]);
			}
			else
			{
				for (int j = 0; j < dim; j++)
					x2.push_back(x[k * dim + j]);
				y2.push_back(y[k]);
			}
		}

		// update according to sigma and mu
		for (int d = 0; d < dim; d++)
		{
			if (sigma[d]) dt->w[d] /= sigma[d];
			dt->b -= dt->w[d] * mu[d];
		}

		dt->left = _Train(x1, y1, mu, sigma, level - 1);
		dt->right = _Train(x2, y2, mu, sigma, level - 1);
		return dt;
	}

	const vector<float> &Classify(const vector<float> &x) const
	{
		if (leaf)
			return dist;

		float y = b;
		for (int i = 0; i < dim; i++)
		{
			//x[i] -= meanx[i];
			//x[i] /= maxabs[i];
			y += x[i] * w[i];
		}
		if (y >= 0)
			return left->Classify(x);
		else
			return right->Classify(x);
	}

	int ClassifyLabel(const vector<float> &x) const
	{
		auto result = Classify(x);
		int m = 0;
		for (int i = 1; i < c; i++)
			if (result[i] > result[m]) m = i;
		return m;   // Note MATLAB begins the index from 1, but we handle this in the mex file
	}
};

// a template class for a random forest, c is the class number
template<int c> struct RF
{
	vector<DT<c> *> dts;

	// Classify the label. We treat feature as a row-major matrix with (length / 64) rows. That is, each feature is 64 dimensions as indicated in struct DT.
	vector<int> ClassifyLabel(const vector<float> &feature) const
	{
		auto dists = Classify(feature);
		int n = feature.size() / DT<c>::dim;
		vector<int> result(n);
		for(int row = 0; row < n; row++)
		{
			int maxDistI = 0;
			for (int i = 1; i < c; i++)
				if (dists[row * c + maxDistI] < dists[row * c + i])
					maxDistI = i;
			result[row] = maxDistI;
		}
		return result;
	}

	vector<float> Classify(const vector<float> &feature) const
	{
		int dim = DT<c>::dim;
		int n = feature.size() / dim;
		vector<float> result(n * c);
        concurrency::parallel_for(0, n, [&](int row)
		//for(int row = 0; row < n; row++)
		{
			vector<float> rowFeature(dim);
			copy(feature.begin() + row * dim, feature.begin() + (row + 1) * dim, rowFeature.begin());
			for_each(dts.begin(), dts.end(), [&](const DT<c> *pdt)
			{
				auto treeDist = pdt->Classify(rowFeature);
				for(int i = 0; i < c; i++) result[row * c + i] += treeDist[i];  // may have numeric issues
			});
		});
		return result;
}	

	void Serialize(ostream &os)
	{
		int count = dts.size();
		os.write((char *)&count, sizeof(int));
		for_each(dts.begin(), dts.end(), [&](const DT<c> *pdt)
		{
			pdt->Serialize(os);
		});
	}

	static RF<c> Train(const vector<float> &features, const vector<int> &labels, int treeNum = 5, int level = 16)
	{
		auto treeTrainingDataSize = (treeNum - 1) * labels.size() / treeNum;
		int dim = DT<c>::dim;
		RF<c> rf;
		// train the trees one by one
		for (int treeId = 0; treeId < treeNum; treeId++)
		{
			// randomly adopt (treeNum - 1) / treeNum examples for training
			vector<float> featuresForTheTree(treeTrainingDataSize * dim);
			vector<int> labelsForTheTree(treeTrainingDataSize);
			vector<int> chosenIdx(labels.size());
			for(int i = 0; i < labels.size(); i++) chosenIdx[i] = i;
			shuffle(chosenIdx.begin(), chosenIdx.end(), default_random_engine(0));
			chosenIdx.resize(treeTrainingDataSize);
			for(int i = 0; i < chosenIdx.size(); i++) 
			{
				for (int j = 0; j < dim; j++) featuresForTheTree[i * dim + j] = features[chosenIdx[i] * dim + j];
				labelsForTheTree[i] = labels[chosenIdx[i]];
			}
			rf.dts.push_back(DT<c>::Train(featuresForTheTree, labelsForTheTree, level));
			cerr << endl;
		}
		return rf;
	}

	static RF<c> ParseFromStream(istream &is)
	{
		RF<c> rf;
		int count = 0;
		is.read((char *)&count, sizeof(int));
		rf.dts.resize(count);
		for(int i = 0; i < count; i++)
		{
			rf.dts[i] = DT<c>::ParseFromStream(is);
		}
		return rf;
	}

	void Dispose()
	{
		for_each(dts.begin(), dts.end(), [&](DT<c> *dpt) { DT<c>::Dispose(dpt); });
		dts.clear();
	}
};
