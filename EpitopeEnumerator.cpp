//============================================================================
// Name        : EpitopeEnumerator.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <assert.h>
#include <utility>
#include <algorithm>

#include "Utilities.h"
#include "readFiles.h"

using namespace std;

std::vector<transcript> getPlusStrandTranscripts(const std::vector<transcript>& transcripts);
void enumeratePeptides(const std::map<std::string, std::string> referenceGenome_plus, const std::vector<transcript>& transcripts_plus, const std::map<std::string, std::map<int, variantFromVCF>>& variants_plus, bool isTumour);
std::tuple<std::string, std::vector<int>, std::vector<std::string>, std::vector<std::vector<int>>> get_reference_and_variantAlleles(const variantFromVCF& v, unsigned int startReferencePos, unsigned int lastReferencePos);

std::set<int> peptideLengths = {8, 9, 10, 11, 15, 16};

int main(int argc, char *argv[]) {

	std::vector<std::string> ARG (argv + 1, argv + argc + !argc);
	std::map<std::string, std::string> arguments;

	arguments["referenceGenome"] = "data/GRCh38_full_analysis_set_plus_decoy_hla.fa.chr20";
	arguments["normalVCF"] = "data/NA12878.vcf.chr20";
	arguments["transcripts"] = "data/gencode.v26.annotation.gff3";

	/*
	std::map<std::pair<int, int>, int> testP;
	testP[make_pair(1,2)] = 4;
	assert(testP.count(make_pair(1,2)));
	assert(! testP.count(make_pair(1,3)));
	std::cout << testP[make_pair(1,2)] << "\n" << std::flush;
	assert(2 == 10);
	 */

	for(unsigned int i = 0; i < ARG.size(); i++)
	{
		if((ARG.at(i).length() > 2) && (ARG.at(i).substr(0, 2) == "--"))
		{
			std::string argname = ARG.at(i).substr(2);
			std::string argvalue = ARG.at(i+1);
			arguments[argname] = argvalue;
		}
	}

	std::vector<transcript> transcripts = readTranscripts(arguments.at("transcripts"));

	std::map<std::string, std::map<int, variantFromVCF>> variants = readVariants(arguments.at("normalVCF"));

	std::map<std::string, std::string> referenceGenome = Utilities::readFASTA(arguments.at("referenceGenome"));

	std::vector<transcript> transcripts_plus = getPlusStrandTranscripts(transcripts);

	for(const transcript& t : transcripts_plus)
	{
		std::string t_referenceSequence;
		for(const transcriptExon& e : t.exons)
		{
			if(e.valid)
			{
				if(referenceGenome.count(t.chromosomeID) == 0)
				{
					std::cerr << "Unknown chromosome ID: "+t.chromosomeID << "\n" << std::flush;
					throw std::runtime_error("Unknown chromosome ID: "+t.chromosomeID);
				}
				const std::string& chromosomeSequence = referenceGenome.at(t.chromosomeID);
				assert(e.firstPos <= e.lastPos);
				assert(e.firstPos >= 0);
				assert(e.lastPos < chromosomeSequence.length());

				t_referenceSequence += referenceGenome.at(t.chromosomeID).substr(e.firstPos, e.lastPos - e.firstPos + 1);
			}
		}
		std::string translation;
		assert((t_referenceSequence.length() % 3) == 0);
		for(unsigned int i = 0; i < t_referenceSequence.length(); i += 3)
		{
			std::string codon = t_referenceSequence.substr(i, 3);
			assert(codon.length() == 3);
			translation += translateCodon2AA(codon);
		}
		if(translation.back() != '!')
		{
			// std::cout << t.transcriptID << " " << translation << "\n" << std::flush;
		}
		// assert(translation.back() == '!');
	}

	// assert("Are the GFF stop coordinates inclusive?" == "");


	enumeratePeptides(referenceGenome, transcripts_plus, variants, true);

	assert(1 == 2);

	assert("minus-strand transcripts!" == "");
	assert("add stop codons!" == "");

	return 0;
}

void enumeratePeptides(const std::map<std::string, std::string> referenceGenome_plus, const std::vector<transcript>& transcripts_plus, const std::map<std::string, std::map<int, variantFromVCF>>& variants_plus, bool isTumour)
{
	int peptideHaplotypeLength = *(peptideLengths.rbegin()) * 2 - 1;
	peptideHaplotypeLength = 4;

	using fragmentT = std::tuple<std::string, std::vector<std::pair<int, int>>, std::vector<bool>>;
	std::map<fragmentT, double> fragments;

	auto printOneFragment = [](const fragmentT& f, double p) -> void {
		std::cout << std::get<0>(f) << "\n";
		for(bool b : std::get<2>(f))
		{
			std::cout << (int)b;
		}
		std::cout << "\n";
		int minPos = -1;
		int maxPos = -1;
		for(auto p : std::get<1>(f))
		{
			assert(p.first <= p.second);
			assert(p.first != -1);
			assert(p.second != -1);

			if(minPos == -1)
			{
				minPos = p.first;
			}
			assert(minPos <= p.first);

			maxPos = p.second;
		}
		std::cout << minPos << " - " << maxPos << "\n\n" << std::flush;
	};

	auto addToTotalFragmentStore = [&fragments](std::map<fragmentT, double>& fragmentsStore_oneExtension) -> void {
		for(auto oneFragment : fragmentsStore_oneExtension)
		{
			// QC
			int lastCodonEnd = -1;
			for(unsigned int pI = 0; pI < std::get<0>(oneFragment.first).size(); pI++)
			{
				assert(
						((std::get<1>(oneFragment.first).at(pI).first == -1) && (std::get<1>(oneFragment.first).at(pI).second == - 1)) ||
						(std::get<1>(oneFragment.first).at(pI).first <= std::get<1>(oneFragment.first).at(pI).second)
				);
				if(std::get<1>(oneFragment.first).at(pI).first != -1)
				{
					if(lastCodonEnd != -1)
					{
						assert(lastCodonEnd < std::get<1>(oneFragment.first).at(pI).first);
					}
					lastCodonEnd = std::get<1>(oneFragment.first).at(pI).second;
				}
			}
			if(! fragments.count(oneFragment.first))
			{
				fragments[oneFragment.first] = 0;
			}
			if(fragments.at(oneFragment.first) < oneFragment.second)
			{
				fragments.at(oneFragment.first) = oneFragment.second;
			}
		}
	};


	auto addToFragmentsFromOneExtension = [](std::map<fragmentT, double>& fragmentsStore, const std::set<fragmentT>& fragmentsThisExtension, double p) -> void {
		for(const fragmentT& fragmentFromExtension : fragmentsThisExtension)
		{
			if(fragmentsStore.count(fragmentFromExtension) == 0)
			{
				fragmentsStore[fragmentFromExtension] = 0;
			}
			fragmentsStore.at(fragmentFromExtension) += p;
		}
	};

	using runningHaplotypeKey = std::tuple<std::string, std::vector<std::pair<int, int>>, std::vector<bool>, std::string, std::vector<bool>, std::vector<int>>;
	using runningHaplotypePairKey = std::pair<runningHaplotypeKey, runningHaplotypeKey>;

	class runningHaplotype {
	public:
		std::string AApart;
		std::vector<std::pair<int, int>> AApart_firstLast;
		std::vector<bool> AApart_interesting;

		std::string nucleotidePart;
		std::vector<bool> nucleotidePart_interesting;
		std::vector<int> nucleotidePart_refCoordinates;
		int nucleotidePart_nonGap;

		runningHaplotypeKey getKey()
		{
			return make_tuple(
					AApart,
					AApart_firstLast,
					AApart_interesting,
					nucleotidePart,
					nucleotidePart_interesting,
					nucleotidePart_refCoordinates
			);
		}

		runningHaplotype()
		{
			nucleotidePart_nonGap = 0;
		}

		void extendWithNucleotides(const std::string& nucleotides, const std::vector<int>& nucleotides_refCoordinates, const std::vector<bool>& nucleotides_interesting)
		{
			assert(nucleotides.size() == nucleotides_refCoordinates.size());
			assert(nucleotides.size() == nucleotides_interesting.size());

			assert(nucleotidePart.size() == nucleotidePart_refCoordinates.size());
			assert(nucleotidePart.size() == nucleotidePart_interesting.size());
			assert((int)countCharacters_noGaps(nucleotidePart) == nucleotidePart_nonGap); // paranoid

			nucleotidePart.insert(nucleotidePart.end(), nucleotides.begin(), nucleotides.end());
			nucleotidePart_refCoordinates.insert(nucleotidePart_refCoordinates.end(), nucleotides_refCoordinates.begin(), nucleotides_refCoordinates.end());
			nucleotidePart_interesting.insert(nucleotidePart_interesting.end(), nucleotides_interesting.begin(), nucleotides_interesting.end());

			bool OK = true;
			int lastRefPos = -2;
			for(int i = 0; i < (int)nucleotidePart_refCoordinates.size(); i++)
			{
				if((lastRefPos == -2) || (nucleotidePart_refCoordinates.at(i) == -1) || (nucleotidePart_refCoordinates.at(i) > lastRefPos))
				{
					if(nucleotidePart_refCoordinates.at(i) != -1)
					{
						lastRefPos = nucleotidePart_refCoordinates.at(i);
					}
				}
				else
				{
					OK = false;
				}
			}

			if(! OK)
			{
				std::cerr << "Problem!\n";
				for(auto p : nucleotidePart_refCoordinates)
				{
					std::cerr << p << " ";
				}
				std::cerr << "\n";
				for(auto p : nucleotides_refCoordinates)
				{
					std::cerr << p << " ";
				}
				std::cerr << "\n" << std::flush;
				throw std::runtime_error("Position problem");
			}

			int added_nonGaps = countCharacters_noGaps(nucleotides);
			nucleotidePart_nonGap += added_nonGaps;

			while(nucleotidePart_nonGap >= 3)
			{
				// find the next codon, its coordinates, and whether it's interesting
				std::string codon; codon.reserve(3);
				int codon_firstPos = -2;
				int codon_lastPos = -2;
				bool codon_interesting = false;
				int consumedIndex_inNucleotidePart = 0;
				while(codon.length() < 3)
				{
					unsigned char charForConsumption = nucleotidePart.at(consumedIndex_inNucleotidePart);
					int refCoordinateForConsumption = nucleotidePart_refCoordinates.at(consumedIndex_inNucleotidePart);

					codon_interesting = (codon_interesting || nucleotidePart_interesting.at(consumedIndex_inNucleotidePart));
					consumedIndex_inNucleotidePart++;
					if((charForConsumption != '-') && (charForConsumption != '_'))
					{
						codon.push_back(charForConsumption);
						if(refCoordinateForConsumption != -1)
						{
							if((codon_firstPos == -2))
								codon_firstPos = refCoordinateForConsumption;
							codon_lastPos = refCoordinateForConsumption;
						}
					}

					assert(consumedIndex_inNucleotidePart <= (int)nucleotidePart.size());
				}

				if(codon_firstPos == -2)
				{
					assert(codon_lastPos == -2);
					codon_firstPos = -1;
					codon_lastPos = -1;
				}
				else
				{
					assert(codon_lastPos >= 0);
				}

				// translate codon -> AA part
				AApart.append(translateCodon2AA(codon));
				AApart_interesting.push_back(codon_interesting);
				if(!(codon_firstPos <= codon_lastPos))
				{
					std::cerr << "codon_firstPos" << ": " << codon_firstPos << "\n";
					std::cerr << "codon_lastPos" << ": " << codon_lastPos << "\n";
					for(auto nP : nucleotidePart_refCoordinates)
					{
						std::cerr << nP << " ";
					}
					std::cerr << "\n" << std::flush;
				}
				assert(codon_firstPos <= codon_lastPos);
				AApart_firstLast.push_back(make_pair(codon_firstPos, codon_lastPos));

				// remove the consumed components
				bool haveRemainder = (consumedIndex_inNucleotidePart < (int)nucleotidePart.size());
				nucleotidePart = haveRemainder ? nucleotidePart.substr(consumedIndex_inNucleotidePart) : "";
				nucleotidePart_interesting = haveRemainder ? std::vector<bool>(nucleotidePart_interesting.begin()+consumedIndex_inNucleotidePart, nucleotidePart_interesting.end()) : std::vector<bool>();
				nucleotidePart_refCoordinates = haveRemainder ? std::vector<int>(nucleotidePart_refCoordinates.begin()+consumedIndex_inNucleotidePart, nucleotidePart_refCoordinates.end()) : std::vector<int>();
				nucleotidePart_nonGap -= 3;

				assert(nucleotidePart.size() == nucleotidePart_refCoordinates.size());
				assert(nucleotidePart.size() == nucleotidePart_interesting.size());
				assert((int)countCharacters_noGaps(nucleotidePart) == nucleotidePart_nonGap); // paranoid
			}
		}

		void shortenAA(int peptideHaplotypeLength, std::set<fragmentT>& fragmentsStore)
		{
			while((int)AApart.length() >= peptideHaplotypeLength)
			{
				std::string extract_AA = AApart.substr(0, peptideHaplotypeLength);
				std::vector<std::pair<int, int>> extract_positions = std::vector<std::pair<int, int>>(AApart_firstLast.begin(), AApart_firstLast.begin()+peptideHaplotypeLength);
				std::vector<bool> extract_interesting = std::vector<bool>(AApart_interesting.begin(), AApart_interesting.begin()+peptideHaplotypeLength);
				assert((int)extract_AA.size() == peptideHaplotypeLength); assert((int)extract_positions.size() == peptideHaplotypeLength); assert((int)extract_interesting.size() == peptideHaplotypeLength);

				AApart = AApart.substr(1);
				AApart_firstLast = std::vector<std::pair<int, int>>(AApart_firstLast.begin()+1, AApart_firstLast.end());
				AApart_interesting = std::vector<bool>(AApart_interesting.begin()+1, AApart_interesting.end());
				assert(AApart.size() == AApart_firstLast.size()); assert(AApart.size() == AApart_interesting.size());

				fragmentT key = make_tuple(extract_AA, extract_positions, extract_interesting);
				fragmentsStore.insert(key);
			}
		}

		void storeRemainingAA(int peptideHaplotypeLength, std::set<fragmentT>& fragmentsStore)
		{
			assert((int)AApart.length() < peptideHaplotypeLength);
			assert(nucleotidePart.length() < 3);
			fragmentT key = make_tuple(AApart, AApart_firstLast, AApart_interesting);
			fragmentsStore.insert(key);
		}
	};

	class runningPairOfHaplotypes {
	public:
		double p;
		runningHaplotype h1;
		runningHaplotype h2;
		runningPairOfHaplotypes()
		{
			p = 1;
		}
		void extendWithNucleotides(const std::string& h1_nucleotides, const std::vector<int>& h1_nucleotides_refCoordinates, const std::vector<bool>& h1_nucleotides_interesting, const std::string& h2_nucleotides, const std::vector<int>& h2_nucleotides_refCoordinates, const std::vector<bool>& h2_nucleotides_interesting)
		{
			h1.extendWithNucleotides(h1_nucleotides, h1_nucleotides_refCoordinates, h1_nucleotides_interesting);
			h2.extendWithNucleotides(h2_nucleotides, h2_nucleotides_refCoordinates, h2_nucleotides_interesting);
		}

		runningHaplotypePairKey getKey()
		{
			return make_pair(h1.getKey(), h2.getKey());
		}
	};


	for(unsigned int transcriptI = 0; transcriptI < transcripts_plus.size(); transcriptI++)
	{
		const transcript& transcript = transcripts_plus.at(transcriptI);
		assert(transcript.strand == '+');
		std::string chromosomeID = transcript.chromosomeID;
		if(referenceGenome_plus.count(chromosomeID) == 0)
			continue;

		int exons = transcript.exons.size();

		// make sure that the exons go from left to right in a non-overlapping fashion
		int lastValidExonI = -1;
		for(int exonI = 0; exonI < exons; exonI++)
		{
			if(transcript.exons.at(exonI).valid)
			{
				assert(transcript.exons.at(exonI).firstPos <= transcript.exons.at(exonI).lastPos);
				if(lastValidExonI != -1)
				{
					assert(transcript.exons.at(lastValidExonI).lastPos < transcript.exons.at(exonI).firstPos);
				}
				lastValidExonI = exonI;
			}
		}

		runningHaplotype referenceHaplotype;
		runningPairOfHaplotypes firstSampleHaplotype;
		std::vector<runningPairOfHaplotypes> sampleHaplotypePairs = {firstSampleHaplotype};

		auto doExtension = [&](std::string referenceSequence, std::vector<int> referenceSequence_refCoordinates, std::vector<std::string> sampleAlleles, std::vector<std::vector<int>> sampleAlleles_refCoordinates, std::vector<bool> sampleAlleles_interesting) -> void {
			assert(sampleAlleles.size() == 2);
			assert(sampleAlleles.size() == sampleAlleles_refCoordinates.size());
			assert(sampleAlleles.size() == sampleAlleles_interesting.size());
			assert(sampleAlleles.at(0).length() == referenceSequence.length());
			assert(sampleAlleles.at(1).length() == referenceSequence.length());

			std::vector<bool> referenceSequence_interesting;
			referenceSequence_interesting.resize(referenceSequence.length(), false);


			referenceHaplotype.extendWithNucleotides(referenceSequence, referenceSequence_refCoordinates, referenceSequence_interesting);

			std::vector<std::vector<bool>> sampleAlleles_perCharacter_interesting;
			sampleAlleles_perCharacter_interesting.reserve(sampleAlleles.size());
			for(const std::string& oneSampleAllele : sampleAlleles)
			{
				std::vector<bool> oneSampleAllele_interesting;
				oneSampleAllele_interesting.resize(oneSampleAllele.length(), false);
				sampleAlleles_perCharacter_interesting.push_back(oneSampleAllele_interesting);
			}


			bool nonIdenticalRefCoordinates = (sampleAlleles_refCoordinates.at(0).size() != sampleAlleles_refCoordinates.at(1).size());
			if(!nonIdenticalRefCoordinates)
			{
				for(unsigned int i = 0; i < sampleAlleles_refCoordinates.at(0).size(); i++)
				{
					if(sampleAlleles_refCoordinates.at(0).at(i) != sampleAlleles_refCoordinates.at(1).at(i))
					{
						nonIdenticalRefCoordinates = true;
						break;
					}
				}
			}
			bool heterozygous = (sampleAlleles.at(0) != sampleAlleles.at(1)) || (sampleAlleles_interesting.at(0) != sampleAlleles_interesting.at(1)) || (nonIdenticalRefCoordinates);
			size_t existingSampleHaplotypes_maxI = sampleHaplotypePairs.size();
			if(! heterozygous)
			{
				assert(sampleAlleles.at(0) == sampleAlleles.at(1));
				for(unsigned int existingHaplotypePairI = 0; existingHaplotypePairI < existingSampleHaplotypes_maxI; existingHaplotypePairI++)
				{
					sampleHaplotypePairs.at(existingHaplotypePairI).h1.extendWithNucleotides(sampleAlleles.at(0), sampleAlleles_refCoordinates.at(0), sampleAlleles_perCharacter_interesting.at(0));
					sampleHaplotypePairs.at(existingHaplotypePairI).h2.extendWithNucleotides(sampleAlleles.at(0), sampleAlleles_refCoordinates.at(0), sampleAlleles_perCharacter_interesting.at(0));
				}
			}
			else
			{
				assert(2 == 4);
				if(existingSampleHaplotypes_maxI == 1)
				{
					sampleHaplotypePairs.at(0).h1.extendWithNucleotides(sampleAlleles.at(0), sampleAlleles_refCoordinates.at(0), sampleAlleles_perCharacter_interesting.at(0));
					sampleHaplotypePairs.at(0).h2.extendWithNucleotides(sampleAlleles.at(1), sampleAlleles_refCoordinates.at(1), sampleAlleles_perCharacter_interesting.at(1));
				}
				else
				{
					for(unsigned int existingHaplotypePairI = 0; existingHaplotypePairI < existingSampleHaplotypes_maxI; existingHaplotypePairI++)
					{
						runningPairOfHaplotypes preExtensionPair = sampleHaplotypePairs.at(existingHaplotypePairI);

						sampleHaplotypePairs.at(existingHaplotypePairI).h1.extendWithNucleotides(sampleAlleles.at(0), sampleAlleles_refCoordinates.at(0), sampleAlleles_perCharacter_interesting.at(0));
						sampleHaplotypePairs.at(existingHaplotypePairI).h2.extendWithNucleotides(sampleAlleles.at(1), sampleAlleles_refCoordinates.at(1), sampleAlleles_perCharacter_interesting.at(1));
						sampleHaplotypePairs.at(existingHaplotypePairI).p *= 0.5;

						for(unsigned int sampleAlleleI = 1; sampleAlleleI < sampleAlleles.size(); sampleAlleleI++)
						{
							sampleHaplotypePairs.push_back(preExtensionPair);

							sampleHaplotypePairs.back().h1.extendWithNucleotides(sampleAlleles.at(1), sampleAlleles_refCoordinates.at(1), sampleAlleles_perCharacter_interesting.at(1));
							sampleHaplotypePairs.back().h2.extendWithNucleotides(sampleAlleles.at(0), sampleAlleles_refCoordinates.at(0), sampleAlleles_perCharacter_interesting.at(0));
							sampleHaplotypePairs.back().p *= 0.5;
						}
					}
				}
			}

			if(sampleHaplotypePairs.size() > 1)
				std::cout << "sampleHaplotypes.size()" << ": " << sampleHaplotypePairs.size() << "\n" << std::flush;

			double p_sum = 0;
			for(auto h : sampleHaplotypePairs)
			{
				p_sum += h.p;
			}
			assert(abs(p_sum - 1) <= 1e-4);

			std::map<fragmentT, double> newFragments_thisExtension;
			for(runningPairOfHaplotypes& hP : sampleHaplotypePairs)
			{
				std::set<fragmentT> thisPair_newFragments;
				hP.h1.shortenAA(peptideHaplotypeLength, thisPair_newFragments);
				hP.h2.shortenAA(peptideHaplotypeLength, thisPair_newFragments);
				addToFragmentsFromOneExtension(newFragments_thisExtension, thisPair_newFragments, hP.p);
			}
			addToTotalFragmentStore(newFragments_thisExtension);

			std::map<runningHaplotypePairKey, unsigned int> havePair;
			std::set<unsigned int> delete_indices;
			for(unsigned int runningHaplotypePairI = 0; runningHaplotypePairI < sampleHaplotypePairs.size(); runningHaplotypePairI++)
			{
				runningPairOfHaplotypes& hP = sampleHaplotypePairs.at(runningHaplotypePairI);
				runningHaplotypePairKey pairKey = hP.getKey();
				if(havePair.count(pairKey))
				{
					sampleHaplotypePairs.at(havePair.at(pairKey)).p += hP.p;
					delete_indices.insert(runningHaplotypePairI);
				}
				else
				{
					havePair[pairKey] = runningHaplotypePairI;
				}
			}

			if(sampleHaplotypePairs.size() > 1)
			{
				std::cout << "sampleHaplotypePairs.size()" << ": " <<  sampleHaplotypePairs.size() << "\n";
				std::cout << "havePair.size()" << ": " <<  havePair.size() << "\n";
				std::cout << std::flush;
			}
			unsigned int sampleHaplotypePairs_size_beforeDeletion = sampleHaplotypePairs.size();

			if(delete_indices.size())
			{
				assert(2 == 4);
				std::cout << "Delete " << delete_indices.size() << "\n" << std::flush;
			}
			if(delete_indices.size() >= 2)
			{
				unsigned int first = *(delete_indices.begin());
				unsigned int last = *(delete_indices.rbegin());
				assert(first < last);
				int lastIndexDeleted = -1;
				for(std::set<unsigned int>::reverse_iterator deleteIndexIt = delete_indices.rbegin(); deleteIndexIt != delete_indices.rend(); deleteIndexIt++)
				{
					unsigned int indexToDelete = *deleteIndexIt;
					assert((lastIndexDeleted == -1) || ((int)indexToDelete < lastIndexDeleted));
					sampleHaplotypePairs.erase(sampleHaplotypePairs.begin() + indexToDelete);
					lastIndexDeleted = (int)indexToDelete;
				}
			}
			assert(sampleHaplotypePairs.size() == (sampleHaplotypePairs_size_beforeDeletion - delete_indices.size()));

			p_sum = 0;
			for(auto h : sampleHaplotypePairs)
			{
				p_sum += h.p;
			}
			assert(abs(p_sum - 1) <= 1e-4);
		};

		//std::string collectedReferenceSequence;
		//std::vector<> runningSampleHaplotypes;

		for(int exonI = 0; exonI < exons; exonI++)
		{
			const transcriptExon& exon = transcript.exons.at(exonI);
			if(exon.valid == false)
				continue;

			for(unsigned int referencePos = exon.firstPos; referencePos <= exon.lastPos; referencePos++)
			{
				//assert(2 == 10);
				//std::cerr << "exon " << exonI << " from " << exon.firstPos << " to " << exon.lastPos << " pos: " << referencePos << "\n" << std::flush;
				if(variants_plus.count(chromosomeID) && variants_plus.at(chromosomeID).count(referencePos))
				{
					//std::cerr << "\tv\n" << std::flush;
					std::tuple<std::string, std::vector<int>, std::vector<std::string>, std::vector<std::vector<int>>> reference_and_variantAlleles = get_reference_and_variantAlleles(variants_plus.at(chromosomeID).at(referencePos), referencePos, exon.lastPos);

					std::vector<bool> extendWith_interesting;
					if(isTumour)
					{
						extendWith_interesting.reserve(std::get<2>(reference_and_variantAlleles).size());
						for(const std::string& sampleAllele : std::get<2>(reference_and_variantAlleles))
						{
							extendWith_interesting.push_back(!(sampleAllele == std::get<0>(reference_and_variantAlleles)));
						}
					}
					else
					{
						extendWith_interesting.resize(std::get<2>(reference_and_variantAlleles).size(), false);
					}

					//std::cout << "V: " << std::get<2>(reference_and_variantAlleles).size() << "\n" << std::flush;

					std::vector<int> referenceAllele_coordinates;
					std::vector<std::vector<int>> sampleAlleles_refCoordinates;
					//std::cerr << "\t" << std::get<0>(reference_and_variantAlleles) << "\n" << std::flush;
					/*
					for(unsigned int vI = 0; vI < std::get<2>(reference_and_variantAlleles).size(); vI++)
					{
						std::cerr << "\t" << std::get<2>(reference_and_variantAlleles).at(vI) << "\n" << std::flush;
						std::cerr << "\t";
						for(auto pI : std::get<3>(reference_and_variantAlleles).at(vI))
						{
							std::cerr << pI << " ";
						}
						std::cerr << "\n" << std::flush;
					}
					*/
					doExtension(std::get<0>(reference_and_variantAlleles), std::get<1>(reference_and_variantAlleles), std::get<2>(reference_and_variantAlleles), std::get<3>(reference_and_variantAlleles), extendWith_interesting);

					int reference_extension_length_noGaps = countCharacters_noGaps(std::get<0>(reference_and_variantAlleles));

					referencePos += (reference_extension_length_noGaps - 1);
				}
				else
				{
					//std::cerr << "\tnV\n" << std::flush;
					std::string referenceCharacter = referenceGenome_plus.at(chromosomeID).substr(referencePos, 1);
					std::vector<std::string> extendWith = {referenceCharacter, referenceCharacter};
					std::vector<int> referenceAllele_coordinates = {(int)referencePos};
					std::vector<std::vector<int>> sampleAlleles_refCoordinates = {{(int)referencePos},{(int)referencePos}};
					std::vector<bool> extendWith_interesting = {false, false};
					doExtension(referenceCharacter, referenceAllele_coordinates, extendWith, sampleAlleles_refCoordinates, extendWith_interesting);
				}
			}
		}


		std::map<fragmentT, double> newFragments_last;
		for(runningPairOfHaplotypes& hP : sampleHaplotypePairs)
		{
			std::set<fragmentT> thisPair_newFragments;
			hP.h1.storeRemainingAA(peptideHaplotypeLength, thisPair_newFragments);
			hP.h2.storeRemainingAA(peptideHaplotypeLength, thisPair_newFragments);
			addToFragmentsFromOneExtension(newFragments_last, thisPair_newFragments, hP.p);
		}
		addToTotalFragmentStore(newFragments_last);

	}

	std::cout << "Have " << fragments.size() << " fragments.\n" << std::flush;

	/*
	unsigned int printFragments = 10;
	unsigned int printedFragments = 0;
	for(auto f : fragments)
	{
		std::cout << "." << std::flush;
		printOneFragment(f.first, f.second);
		printedFragments++;
		std::cout << printedFragments << " " << printFragments << " " << (printedFragments == printFragments) << "\n" << std::flush;
		if(printedFragments == printFragments)
			break;
	}

	*/

	assert(1 == 11);
}

std::tuple<std::string, std::vector<int>, std::vector<std::string>, std::vector<std::vector<int>>> get_reference_and_variantAlleles(const variantFromVCF& v, unsigned int startReferencePos, unsigned int lastReferencePos)
{
	assert(v.position <= lastReferencePos);

	// check that we have variant alleles and that they are "aligned"
	assert(v.sampleAlleles.size());
	for(const std::string& variantAllele : v.sampleAlleles)
	{
		assert(variantAllele.length() == v.referenceString.length());
	}

	auto getReferencePositionVector = [](const std::string& S, unsigned int firstPos) -> std::vector<int> {
		std::vector<int> forReturn;
		forReturn.resize(S.length(), -1);
		unsigned int runningRefPosition = firstPos;
		for(unsigned int i = 0; i < S.length(); i++)
		{
			unsigned char thisC = S.at(i);
			if((thisC != '-') && (thisC != '_'))
			{
				forReturn.at(i) = runningRefPosition;
				runningRefPosition++;
			}
		}

		int lastRefPos = -2;
		for(int i = 0; i < forReturn.size(); i++)
		{
			assert((lastRefPos == -2) || (forReturn.at(i) == -1) || (forReturn.at(i) > lastRefPos));
			if(forReturn.at(i) != -1)
			{
				lastRefPos = forReturn.at(i);
			}
			// assert(forReturn.at(i-1) <= forReturn.at(i));
		}
		return forReturn;
	};

	assert(v.position == startReferencePos);
	// if everything within last-position constraints, do nothing ...
	unsigned int v_referenceString_length_noGaps = countCharacters_noGaps(v.referenceString);
	if((v.position + v_referenceString_length_noGaps - 1) <= lastReferencePos)
	{
		std::vector<std::vector<int>> referencePositions_per_sampleAllele;
		referencePositions_per_sampleAllele.reserve(v.sampleAlleles.size());
		std::vector<int> referencePositionVector = getReferencePositionVector(v.referenceString, startReferencePos);
		for(const std::string& sampleAllele : v.sampleAlleles)
		{
			referencePositions_per_sampleAllele.push_back(referencePositionVector);
		}
		return make_tuple(v.referenceString, referencePositionVector, v.sampleAlleles, referencePositions_per_sampleAllele);
	}
	// else: cut variant alleles accordingly
	else
	{
		std::string forReturn_ref;
		std::vector<std::string> forReturn_alleles;
		forReturn_alleles.resize(v.sampleAlleles.size());

		int consumePositions = lastReferencePos - v.position + 1;
		assert(consumePositions >= 1);
		int consumedPositions = 0;
		int consumeI = 0;
		while(consumedPositions < consumePositions)
		{
			unsigned char refC = v.referenceString.at(consumeI);

			forReturn_ref.push_back(refC);
			for(unsigned int alternativeAlleleI = 0; alternativeAlleleI < v.sampleAlleles.size(); alternativeAlleleI++)
			{
				forReturn_alleles.at(alternativeAlleleI).push_back(v.sampleAlleles.at(alternativeAlleleI).at(consumeI));
			}
			if((refC != '-') && (refC != '_'))
			{
				consumedPositions++;
			}
			consumeI++;
		}

		for(const std::string& alternativeAllele : forReturn_alleles)
		{
			assert(alternativeAllele.size() == forReturn_ref.length());
		}

		std::vector<std::vector<int>> referencePositions_per_returnAllele;
		referencePositions_per_returnAllele.reserve(v.sampleAlleles.size());

		std::vector<int> referencePositionVector = getReferencePositionVector(forReturn_ref, startReferencePos);

		for(const std::string& sampleAllele : forReturn_alleles)
		{
			referencePositions_per_returnAllele.push_back(referencePositionVector);
		}

		return make_tuple(forReturn_ref, referencePositionVector, forReturn_alleles, referencePositions_per_returnAllele);
	}
}

std::vector<transcript> getPlusStrandTranscripts(const std::vector<transcript>& transcripts)
{
	std::vector<transcript> forReturn;
	forReturn.reserve(transcripts.size());
	for(transcript t : transcripts)
	{
		if(t.strand == '+')
		{
			forReturn.push_back(t);
		}
	}

	std::cout << "getPlusStrandTranscripts(..): " << forReturn.size() << " plus-strand transcripts.\n" << std::flush;

	return forReturn;
}



