// Loads CharacterButes.TXT with the rewritten ButeMgr and prints each character's model and skins,
// resolved the way CHierarchicalButeMgr::GetString does (own value, else the Parent chain).
#include <stdio.h>
#include <vector>
#include <string>
#include "butemgr.h"

static CButeMgr g_bm;

static CString HGet(const char* tag, const char* att, int depth = 0)
{
	if (!tag || !*tag || depth > 20) return CString("");
	if (g_bm.Exist(tag, att)) return g_bm.GetString(tag, att);
	CString parent = g_bm.GetString(tag, "Parent", CString(""));
	if (parent.CompareNoCase(tag) == 0) return CString("");
	return HGet(parent, att, depth + 1);
}

static bool CollectTag(const char* name, void* ctx)
{
	((std::vector<std::string>*)ctx)->push_back(name);
	return true;
}

int main(int argc, char** argv)
{
	// butetest -order <file>: print tags in GetTags order (the index game code numbers them by)
	if (argc == 3 && strcmp(argv[1], "-order") == 0)
	{
		if (!g_bm.Parse(CString(argv[2]))) { printf("parse failed\n"); return 1; }
		std::vector<std::string> order;
		g_bm.GetTags(CollectTag, &order);
		for (size_t i = 0; i < order.size(); ++i)
			printf("%zu %s\n", i, order[i].c_str());
		return 0;
	}

	// butetest -roundtrip <in> <out>: parse and save again, to check the file format
	if (argc == 4 && strcmp(argv[1], "-roundtrip") == 0)
	{
		if (!g_bm.Parse(CString(argv[2]))) { printf("parse failed\n"); return 1; }
		printf("LevelStatus in [Marine] as DWORD: %lu\n", (unsigned long)g_bm.GetDword("Marine", "LevelStatus", 12345));
		return g_bm.Save(argv[3]) ? 0 : 1;
	}

	if (!g_bm.Parse(CString(argv[1]))) { printf("parse failed\n"); return 1; }
	std::vector<std::string> tags;
	g_bm.GetTags(CollectTag, &tags);
	for (auto& t : tags)
	{
		CString model = HGet(t.c_str(), "DefaultModel");
		if (model.IsEmpty()) continue;
		printf("[%s] model=%s%s skindir=%s", t.c_str(), (LPCSTR)HGet(t.c_str(), "DefaultModelDir"), (LPCSTR)model,
			(LPCSTR)HGet(t.c_str(), "DefaultSkinDir"));
		for (int i = 0; i < 4; ++i)
		{
			char k[32];
			sprintf(k, "DefaultSkin%d", i);
			printf(" s%d=%s", i, (LPCSTR)HGet(t.c_str(), k));
		}
		printf("\n");
	}
	return 0;
}
