#include "RE/Fallout.h"

bool* LookupREFRByHandle(const uint32_t& handleIn, RE::NiPointer<RE::TESObjectREFR>& ref) {
	using func_t = decltype(LookupREFRByHandle);
	static REL::Relocation<func_t> func{ REL::VariantOffset(0x0000AB60, 0x0024C240, 0) };

	return func(handleIn, ref);
	}


struct ViewCasterUpdateEvent_MA {
	struct ViewCasterValueEvent_MA {
		uint32_t			crosshairHandler;	// 00	RefHandler
		uint32_t			unk04;				// 04
		uint32_t			unk08;				// 08
		uint32_t			unk0C;				// 0C
		uint64_t			unk10;				// 10
		uint64_t			unk18;				// 18
		uint32_t			unk20;				// 20
		uint32_t			unk24;				// 24
		};
	ViewCasterValueEvent_MA value;			// 00
	bool					unk28;			// 28
	uint32_t				unk2C;			// 2C
	bool					isNPC;			// 30 ?
	float					unk34;			// 34
	bool					canInteractive;	// 38
	};

typedef RE::BSTGlobalEvent::EventSource<ViewCasterUpdateEvent_MA> EventSource;
REL::Relocation<uint64_t*> g_Source(REL::VariantOffset(0x05A5F758, 0x02FF12F0, 0));

static uint32_t CrosshairRefHandle = 0;		// Zero if the crosshair is no longer on a reference
static uint32_t CrosshairActorHandle = 0;		// Memorize the last one until a new one is pointed at

static RE::Actor* lastActor;

class CrosshairChangeHandler_MA : public RE::BSTEventSink<ViewCasterUpdateEvent_MA> {
	public:
		virtual ~CrosshairChangeHandler_MA() {};

	virtual RE::BSEventNotifyControl ProcessEvent(const ViewCasterUpdateEvent_MA &evn, RE::BSTEventSource<ViewCasterUpdateEvent_MA>* ) override {
		RE::NiPointer<RE::TESObjectREFR> pRef = nullptr;
		CrosshairRefHandle = evn.value.crosshairHandler;
		LookupREFRByHandle(CrosshairRefHandle, pRef);
			
		if (pRef != nullptr) {
			if (pRef->formType == RE::ENUM_FORM_ID::kACHR) {
				RE::Actor* a = (RE::Actor*) pRef.get();
				if (a) {
					lastActor = a;
					}
				}

			}
		return RE::BSEventNotifyControl::kContinue;
		}

	static void Register() {
		static CrosshairChangeHandler_MA* pCrosshairHandler = new CrosshairChangeHandler_MA();
		uint64_t* pt = (uint64_t *) g_Source.address();

		EventSource* e = (EventSource*)*pt;

		logger::info("g_source: pt: {:p} *pt = {:X} e: {:p} ",  (void*) pt, *pt, (void*) e);

		e->RegisterSink(pCrosshairHandler);
		CrosshairRefHandle = 0;
		CrosshairActorHandle = 0;
		}
	};


void RegisterCrosshair() {
	CrosshairChangeHandler_MA::Register();
	}

std::vector<RE::Actor*> GetLastCrossHairActor(std::monostate) {
	std::vector<RE::Actor*> result;

	if (lastActor) {
		result.push_back(lastActor);
		}

	return result;
	}
