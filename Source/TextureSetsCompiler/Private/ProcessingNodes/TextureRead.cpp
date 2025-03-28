// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

#include "ProcessingNodes/TextureRead.h"

#include "TextureSetProcessingGraph.h"
#include "TextureSetsHelpers.h"

FTextureRead::FTextureRead(FName SourceNameIn, const FTextureSetSourceTextureDef& SourceDefinitionIn)
	: SourceName(SourceNameIn)
	, SourceDefinition(SourceDefinitionIn)
	, bPrepared(false)
	, AsyncSource()
	, ValidChannels(0)
	, ChannelSwizzle{0, 1, 2, 3}
	, Width(1)
	, Height(1)
	, Slices(1)
{
}

void FTextureRead::ComputeGraphHash(FHashBuilder& HashBuilder) const
{
	HashBuilder << GetNodeTypeName();
	HashBuilder << SourceName;
	HashBuilder << GetTypeHash(SourceDefinition);
}

void FTextureRead::ComputeDataHash(const FTextureSetProcessingContext& Context, FHashBuilder& HashBuilder) const
{
	check(IsInGameThread());

	#if TS_SOFT_SOURCE_TEXTURE_REF
	check(!IsLoading()); // LoadSynchronous often fails if we are loading something else
	#endif

	if(Context.SourceTextures.Contains(SourceName))
	{
		const FTextureSetSourceTextureReference& TextureRef = Context.SourceTextures.FindChecked(SourceName);

		if (!TextureRef.IsNull())
		{
			FString PayloadIdString;

			// If referenced texture is already loaded just grab the ID string from it directly
			if (!TextureRef.Valid() || !TextureSetsHelpers::GetSourceDataIdAsString(TextureRef.Texture.Get(), PayloadIdString))
			{
#if TS_SOFT_SOURCE_TEXTURE_REF
				// If the referenced texture is not loaded, check if we have the IdString saved with it's asset data
				// so we can avoid loading the texture just to check if it's changed.
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(TextureRef.GetTexturePath());

				if(!TextureSetsHelpers::GetSourceDataIdAsString(AssetData, PayloadIdString))
#endif
				{
					// If we were not able to get the ID string from the asset data, we need to load the texture to retreive it.
					// This should only happen on textures that were created before the texture-sets plugin was enabled, and haven't been re-saved since.
					// As existing textures are modified and re-saved, this should happen less frequently.
					UTexture* T = TextureRef.GetTexture();
					check(IsValid(T));
					FGuid PersistentID = T->Source.GetPersistentId();
					TextureSetsHelpers::GetSourceDataIdAsString(T, PayloadIdString);
				}
			}
			
			check(!PayloadIdString.IsEmpty())
			HashBuilder << PayloadIdString;
			HashBuilder << TextureRef.ChannelMask;
		}
	}
}

void FTextureRead::Prepare(const FTextureSetProcessingContext& Context)
{
	if (bPrepared)
		return;

	if(Context.SourceTextures.Contains(SourceName))
	{
		const FTextureSetSourceTextureReference& TextureRef = Context.SourceTextures.FindChecked(SourceName);
		UTexture* Texture = TextureRef.GetTexture();
		ChannelMask = TextureRef.ChannelMask;

		if(IsValid(Texture))
		{
			check(Texture->Source.IsValid());
			AsyncSource = Texture->Source.CopyTornOff(); // This copies information required to make a safe IO load async.

			switch (AsyncSource.GetFormat())
			{
			case ETextureSourceFormat::TSF_G8:
			case ETextureSourceFormat::TSF_G16:
			case ETextureSourceFormat::TSF_R16F:
			case ETextureSourceFormat::TSF_R32F:
				ValidChannels = 1;
				break;
			case ETextureSourceFormat::TSF_BGRA8:
			case ETextureSourceFormat::TSF_BGRE8:
			case ETextureSourceFormat::TSF_RGBA16:
			case ETextureSourceFormat::TSF_RGBA16F:
			case ETextureSourceFormat::TSF_RGBA32F:
				ValidChannels = 4;
				break;
			default:
				unimplemented();
			}

			uint8 CurrentChannel = 0;

			// Start at the first valid, unmasked channel
			while((CurrentChannel + 1 < ValidChannels) && !(ChannelMask & (1 << CurrentChannel)))
				CurrentChannel++;

			for (int i = 0; i < SourceDefinition.ChannelCount; i++)
			{
				ChannelSwizzle[i] = CurrentChannel;

				// Advance to the next valid, unmasked channel
				for (uint8 NextChannel = CurrentChannel + 1; NextChannel < ValidChannels; NextChannel++)
				{
					if (ChannelMask & (1 << NextChannel))
					{
						CurrentChannel = NextChannel;
						break;
					}
				}
			}

			Width = AsyncSource.GetSizeX();
			Height = AsyncSource.GetSizeY();
			Slices = (SourceDefinition.IsVolume() || SourceDefinition.IsArray()) ? AsyncSource.GetNumSlices() : 1;
			TextureSourceFormat = AsyncSource.GetFormat();
			TextureSourceGamma = AsyncSource.GetGammaSpace(0);
		}
	}

	bPrepared = true;
}

ITextureProcessingNode::FTextureDimension FTextureRead::GetTextureDimension() const
{
	check(bPrepared);
	FTextureDimension Dim;
	Dim.Width = Width;
	Dim.Height = Height;
	Dim.Slices = Slices;
	Dim.Mips = 1; // TODO: Support remapping mips from sub-images
	return Dim;
}

void FTextureRead::Cache()
{
	check(bPrepared); // Should not happen unless called out of order

	if (AsyncSource.IsValid())
	{
		// This version of GetMipData does not make any internal copies,
		// and gives us read-only access to the internal shared buffer.
		// According to the comments, the FSharedBuffer is ref counted
		// so will remain valid as long as we need it
		FTextureSource::FMipData MipData = AsyncSource.GetMipData(nullptr);
		TextureSourceMip0 = MipData.GetMipData(0, 0, 0);
	}
}

namespace
{
	/**
	 * Pow table for fast FColor -> FLinearColor conversion.
	 *
	 * FMath::Pow( i / 255.f, 2.2f )
	 */
	float Pow22OneOver255Table[256] =
	{
		0.0f, 5.07705190066176E-06f, 2.33280046660989E-05f, 5.69217657121931E-05f, 0.000107187362341244f, 0.000175123977503027f, 0.000261543754548491f, 0.000367136269815943f, 0.000492503787191433f,
		0.000638182842167022f, 0.000804658499513058f, 0.000992374304074325f, 0.0012017395224384f, 0.00143313458967186f, 0.00168691531678928f, 0.00196341621339647f, 0.00226295316070643f,
		0.00258582559623417f, 0.00293231832393836f, 0.00330270303200364f, 0.00369723957890013f, 0.00411617709328275f, 0.00455975492252602f, 0.00502820345685554f, 0.00552174485023966f,
		0.00604059365484981f, 0.00658495738258168f, 0.00715503700457303f, 0.00775102739766061f, 0.00837311774514858f, 0.00902149189801213f, 0.00969632870165823f, 0.0103978022925553f,
		0.0111260823683832f, 0.0118813344348137f, 0.0126637200315821f, 0.0134733969401426f, 0.0143105193748841f, 0.0151752381596252f, 0.0160677008908869f, 0.01698805208925f, 0.0179364333399502f,
		0.0189129834237215f, 0.0199178384387857f, 0.0209511319147811f, 0.0220129949193365f, 0.0231035561579214f, 0.0242229420675342f, 0.0253712769047346f, 0.0265486828284729f, 0.027755279978126f,
		0.0289911865471078f, 0.0302565188523887f, 0.0315513914002264f, 0.0328759169483838f, 0.034230206565082f, 0.0356143696849188f, 0.0370285141619602f, 0.0384727463201946f, 0.0399471710015256f,
		0.0414518916114625f, 0.0429870101626571f, 0.0445526273164214f, 0.0461488424223509f, 0.0477757535561706f, 0.049433457555908f, 0.0511220500564934f, 0.052841625522879f, 0.0545922772817603f,
		0.0563740975519798f, 0.0581871774736854f, 0.0600316071363132f, 0.0619074756054558f, 0.0638148709486772f, 0.0657538802603301f, 0.0677245896854243f, 0.0697270844425988f, 0.0717614488462391f,
		0.0738277663277846f, 0.0759261194562648f, 0.0780565899581019f, 0.080219258736215f, 0.0824142058884592f, 0.0846415107254295f, 0.0869012517876603f, 0.0891935068622478f, 0.0915183529989195f,
		0.0938758665255778f, 0.0962661230633397f, 0.0986891975410945f, 0.1011451642096f, 0.103634096655137f, 0.106156067812744f, 0.108711149979039f, 0.11129941482466f, 0.113920933406333f,
		0.116575776178572f, 0.119264013005047f, 0.121985713169619f, 0.124740945387051f, 0.127529777813422f, 0.130352278056244f, 0.1332085131843f, 0.136098549737202f, 0.139022453734703f,
		0.141980290685736f, 0.144972125597231f, 0.147998022982685f, 0.151058046870511f, 0.154152260812165f, 0.157280727890073f, 0.160443510725344f, 0.16364067148529f, 0.166872271890766f,
		0.170138373223312f, 0.173439036332135f, 0.176774321640903f, 0.18014428915439f, 0.183548998464951f, 0.186988508758844f, 0.190462878822409f, 0.193972167048093f, 0.19751643144034f,
		0.201095729621346f, 0.204710118836677f, 0.208359655960767f, 0.212044397502288f, 0.215764399609395f, 0.219519718074868f, 0.223310408341127f, 0.227136525505149f, 0.230998124323267f,
		0.23489525921588f, 0.238827984272048f, 0.242796353254002f, 0.24680041960155f, 0.2508402364364f, 0.254915856566385f, 0.259027332489606f, 0.263174716398492f, 0.267358060183772f,
		0.271577415438375f, 0.275832833461245f, 0.280124365261085f, 0.284452061560024f, 0.288815972797219f, 0.293216149132375f, 0.297652640449211f, 0.302125496358853f, 0.306634766203158f,
		0.311180499057984f, 0.315762743736397f, 0.32038154879181f, 0.325036962521076f, 0.329729032967515f, 0.334457807923889f, 0.339223334935327f, 0.344025661302187f, 0.348864834082879f,
		0.353740900096629f, 0.358653905926199f, 0.363603897920553f, 0.368590922197487f, 0.373615024646202f, 0.37867625092984f, 0.383774646487975f, 0.388910256539059f, 0.394083126082829f,
		0.399293299902674f, 0.404540822567962f, 0.409825738436323f, 0.415148091655907f, 0.420507926167587f, 0.425905285707146f, 0.43134021380741f, 0.436812753800359f, 0.442322948819202f,
		0.44787084180041f, 0.453456475485731f, 0.45907989242416f, 0.46474113497389f, 0.470440245304218f, 0.47617726539744f, 0.481952237050698f, 0.487765201877811f, 0.493616201311074f,
		0.49950527660303f, 0.505432468828216f, 0.511397818884879f, 0.517401367496673f, 0.523443155214325f, 0.529523222417277f, 0.535641609315311f, 0.541798355950137f, 0.547993502196972f,
		0.554227087766085f, 0.560499152204328f, 0.566809734896638f, 0.573158875067523f, 0.579546611782525f, 0.585972983949661f, 0.592438030320847f, 0.598941789493296f, 0.605484299910907f,
		0.612065599865624f, 0.61868572749878f, 0.625344720802427f, 0.632042617620641f, 0.638779455650817f, 0.645555272444934f, 0.652370105410821f, 0.659223991813387f, 0.666116968775851f,
		0.673049073280942f, 0.680020342172095f, 0.687030812154625f, 0.694080519796882f, 0.701169501531402f, 0.708297793656032f, 0.715465432335048f, 0.722672453600255f, 0.729918893352071f,
		0.737204787360605f, 0.744530171266715f, 0.751895080583051f, 0.759299550695091f, 0.766743616862161f, 0.774227314218442f, 0.781750677773962f, 0.789313742415586f, 0.796916542907978f,
		0.804559113894567f, 0.81224148989849f, 0.819963705323528f, 0.827725794455034f, 0.835527791460841f, 0.843369730392169f, 0.851251645184515f, 0.859173569658532f, 0.867135537520905f,
		0.875137582365205f, 0.883179737672745f, 0.891262036813419f, 0.899384513046529f, 0.907547199521614f, 0.915750129279253f, 0.923993335251873f, 0.932276850264543f, 0.940600707035753f,
		0.948964938178195f, 0.957369576199527f, 0.96581465350313f, 0.974300202388861f, 0.982826255053791f, 0.99139284359294f, 1.0f
	};

	/**
	* Table for fast FColor -> FLinearColor conversion.
	*
	* Color > 0.04045 ? pow( Color * (1.0 / 1.055) + 0.0521327, 2.4 ) : Color * (1.0 / 12.92);
	*/
	float sRGBToLinearTable[256] =
	{
		0.0f,
		0.000303526983548838f, 0.000607053967097675f, 0.000910580950646512f, 0.00121410793419535f, 0.00151763491774419f,
		0.00182116190129302f, 0.00212468888484186f, 0.0024282158683907f, 0.00273174285193954f, 0.00303526983548838f,
		0.00334653564113713f, 0.00367650719436314f, 0.00402471688178252f, 0.00439144189356217f, 0.00477695332960869f,
		0.005181516543916f, 0.00560539145834456f, 0.00604883284946662f, 0.00651209061157708f, 0.00699540999852809f,
		0.00749903184667767f, 0.00802319278093555f, 0.0085681254056307f, 0.00913405848170623f, 0.00972121709156193f,
		0.0103298227927056f, 0.0109600937612386f, 0.0116122449260844f, 0.012286488094766f, 0.0129830320714536f,
		0.0137020827679224f, 0.0144438433080002f, 0.0152085141260192f, 0.0159962930597398f, 0.0168073754381669f,
		0.0176419541646397f, 0.0185002197955389f, 0.0193823606149269f, 0.0202885627054049f, 0.0212190100154473f,
		0.0221738844234532f, 0.02315336579873f, 0.0241576320596103f, 0.0251868592288862f, 0.0262412214867272f,
		0.0273208912212394f, 0.0284260390768075f, 0.0295568340003534f, 0.0307134432856324f, 0.0318960326156814f,
		0.0331047661035236f, 0.0343398063312275f, 0.0356013143874111f, 0.0368894499032755f, 0.0382043710872463f,
		0.0395462347582974f, 0.0409151963780232f, 0.0423114100815264f, 0.0437350287071788f, 0.0451862038253117f,
		0.0466650857658898f, 0.0481718236452158f, 0.049706565391714f, 0.0512694577708345f, 0.0528606464091205f,
		0.0544802758174765f, 0.0561284894136735f, 0.0578054295441256f, 0.0595112375049707f, 0.0612460535624849f,
		0.0630100169728596f, 0.0648032660013696f, 0.0666259379409563f, 0.0684781691302512f, 0.070360094971063f,
		0.0722718499453493f, 0.0742135676316953f, 0.0761853807213167f, 0.0781874210336082f, 0.0802198195312533f,
		0.0822827063349132f, 0.0843762107375113f, 0.0865004612181274f, 0.0886555854555171f, 0.0908417103412699f,
		0.0930589619926197f, 0.0953074657649191f, 0.0975873462637915f, 0.0998987273569704f, 0.102241732185838f,
		0.104616483176675f, 0.107023102051626f, 0.109461709839399f, 0.1119324268857f, 0.114435372863418f,
		0.116970666782559f, 0.119538426999953f, 0.122138771228724f, 0.124771816547542f, 0.127437679409664f,
		0.130136475651761f, 0.132868320502552f, 0.135633328591233f, 0.138431613955729f, 0.141263290050755f,
		0.144128469755705f, 0.147027265382362f, 0.149959788682454f, 0.152926150855031f, 0.155926462553701f,
		0.158960833893705f, 0.162029374458845f, 0.16513219330827f, 0.168269398983119f, 0.171441099513036f,
		0.174647402422543f, 0.17788841473729f, 0.181164242990184f, 0.184474993227387f, 0.187820771014205f,
		0.191201681440861f, 0.194617829128147f, 0.198069318232982f, 0.201556252453853f, 0.205078735036156f,
		0.208636868777438f, 0.212230756032542f, 0.215860498718652f, 0.219526198320249f, 0.223227955893977f,
		0.226965872073417f, 0.23074004707378f, 0.23455058069651f, 0.238397572333811f, 0.242281120973093f,
		0.246201325201334f, 0.250158283209375f, 0.254152092796134f, 0.258182851372752f, 0.262250655966664f,
		0.266355603225604f, 0.270497789421545f, 0.274677310454565f, 0.278894261856656f, 0.283148738795466f,
		0.287440836077983f, 0.291770648154158f, 0.296138269120463f, 0.300543792723403f, 0.304987312362961f,
		0.309468921095997f, 0.313988711639584f, 0.3185467763743f, 0.323143207347467f, 0.32777809627633f,
		0.332451534551205f, 0.337163613238559f, 0.341914423084057f, 0.346704054515559f, 0.351532597646068f,
		0.356400142276637f, 0.361306777899234f, 0.36625259369956f, 0.371237678559833f, 0.376262121061519f,
		0.381326009488037f, 0.386429431827418f, 0.39157247577492f, 0.396755228735618f, 0.401977777826949f,
		0.407240209881218f, 0.41254261144808f, 0.417885068796976f, 0.423267667919539f, 0.428690494531971f,
		0.434153634077377f, 0.439657171728079f, 0.445201192387887f, 0.450785780694349f, 0.456411021020965f,
		0.462076997479369f, 0.467783793921492f, 0.473531493941681f, 0.479320180878805f, 0.485149937818323f,
		0.491020847594331f, 0.496932992791578f, 0.502886455747457f, 0.50888131855397f, 0.514917663059676f,
		0.520995570871595f, 0.527115123357109f, 0.533276401645826f, 0.539479486631421f, 0.545724458973463f,
		0.552011399099209f, 0.558340387205378f, 0.56471150325991f, 0.571124827003694f, 0.577580437952282f,
		0.584078415397575f, 0.590618838409497f, 0.597201785837643f, 0.603827336312907f, 0.610495568249093f,
		0.617206559844509f, 0.623960389083534f, 0.630757133738175f, 0.637596871369601f, 0.644479679329661f,
		0.651405634762384f, 0.658374814605461f, 0.665387295591707f, 0.672443154250516f, 0.679542466909286f,
		0.686685309694841f, 0.693871758534824f, 0.701101889159085f, 0.708375777101046f, 0.71569349769906f,
		0.723055126097739f, 0.730460737249286f, 0.737910405914797f, 0.745404206665559f, 0.752942213884326f,
		0.760524501766589f, 0.768151144321824f, 0.775822215374732f, 0.783537788566466f, 0.791297937355839f,
		0.799102735020525f, 0.806952254658248f, 0.81484656918795f, 0.822785751350956f, 0.830769873712124f,
		0.838799008660978f, 0.846873228412837f, 0.854992605009927f, 0.863157210322481f, 0.871367116049835f,
		0.879622393721502f, 0.887923114698241f, 0.896269350173118f, 0.904661171172551f, 0.913098648557343f,
		0.921581853023715f, 0.930110855104312f, 0.938685725169219f, 0.947306533426946f, 0.955973349925421f,
		0.964686244552961f, 0.973445287039244f, 0.982250546956257f, 0.991102093719252f, 1.0f
	};

	template<ETextureSourceFormat SourceFormat>
	static constexpr int GetPixelStride()
	{
		switch (SourceFormat)
		{
		case TSF_G8:
		case TSF_G16:
		case TSF_R16F:
		case TSF_R32F:
			return 1;
		case TSF_BGRA8:
		case TSF_BGRE8:
		case TSF_RGBA16:
		case TSF_RGBA16F:
		case TSF_RGBA32F:
		default:
			return 4;
		}
	}

	template<ETextureSourceFormat SourceFormat>
	FORCEINLINE static int RemapChannel(int Channel)
	{
		static constexpr int RRRRMap[] = { 0, 0, 0, 0 };
		static constexpr int BGRAMap[] = { 2, 1, 0, 3 };
		static constexpr int RGBAMap[] = { 0, 1, 2, 3 };

		switch (SourceFormat)
		{
		case TSF_G8:
		case TSF_G16:
		case TSF_R16F:
		case TSF_R32F:
			return RRRRMap[Channel];
		case TSF_BGRA8:
		case TSF_BGRE8:
			return BGRAMap[Channel];
		case TSF_RGBA16:
		case TSF_RGBA16F:
		case TSF_RGBA32F:
			return RGBAMap[Channel];
		default:
			return RRRRMap[Channel];
		}
	}

	template <typename Lambda>
	static void IterateData(const FTextureDataTileDesc& DestChannel, const FTextureDataTileDesc& SourceChannel, const Lambda& Func)
	{
		// Validate that source and dest tiles are the same size, so we can iterate them together.
		check(SourceChannel.TileSize == DestChannel.TileSize);

		int64 ISource = SourceChannel.TileDataOffset;
		int64 IDest = DestChannel.TileDataOffset;
			
		for (int32 Z = 0; Z < DestChannel.TileSize.Z; Z++)
		{
			for (int32 Y = 0; Y < DestChannel.TileSize.Y; Y++)
			{
				for (int32 X = 0; X < DestChannel.TileSize.X; X++)
				{
					Func(ISource, IDest);

					// Next Pixel
					ISource += SourceChannel.TileDataStepSize.X;
					IDest += DestChannel.TileDataStepSize.X;
				}

				// Next row
				ISource += SourceChannel.TileDataStepSize.Y;
				IDest += DestChannel.TileDataStepSize.Y;
			}

			// Next slice
			ISource += SourceChannel.TileDataStepSize.Z;
			IDest += DestChannel.TileDataStepSize.Z;
		}
	}

	template<ETextureSourceFormat SourceFormat, typename TPixelType>
	void CopyImageData(FSharedBuffer SourceBuffer, int SourceChannelIndex, EGammaSpace GammaSpace, const FTextureDataTileDesc& DestTile, float* DestData)
	{
		const uint64 ExpectedSize = sizeof(TPixelType) * DestTile.TextureSize.X * DestTile.TextureSize.Y * DestTile.TextureSize.Z * GetPixelStride<SourceFormat>();
		check(SourceBuffer.GetSize() >= ExpectedSize);
		check(SourceBuffer.GetSize() % ExpectedSize == 0);

		const FIntVector3 DataStride = FTextureDataTileDesc::ComputeDataStrides(GetPixelStride<SourceFormat>(), DestTile.TextureSize);
		const int32 DataOffset = FTextureDataTileDesc::ComputeDataOffset(DestTile.TileOffset, DataStride) + RemapChannel<SourceFormat>(SourceChannelIndex);

		const FTextureDataTileDesc SourceTile(
			DestTile.TextureSize,
			DestTile.TileSize,
			DestTile.TileOffset,
			DataStride,
			DataOffset
		);

		const TPixelType* SourceData = (TPixelType*)SourceBuffer.GetData();

		if constexpr (std::is_same<float, TPixelType>::value || std::is_same<FFloat16, TPixelType>::value)
		{
			IterateData(DestTile, SourceTile, [&DestData, &SourceData](int64 ISource, int64 IDest)
			{
				DestData[IDest] = SourceData[ISource];
			});

		}
		else if constexpr (std::is_same<uint8, TPixelType>::value)
		{
			switch (GammaSpace)
			{
			case EGammaSpace::Linear:
				IterateData(DestTile, SourceTile, [&DestData, &SourceData](int64 ISource, int64 IDest)
				{
					DestData[IDest] = (float)SourceData[ISource] / 255.f;
				});
				break;
			case EGammaSpace::sRGB:
				IterateData(DestTile, SourceTile, [&DestData, &SourceData](int64 ISource, int64 IDest)
				{
					DestData[IDest] = sRGBToLinearTable[SourceData[ISource]];
				});
				break;
			case EGammaSpace::Pow22:
				IterateData(DestTile, SourceTile, [&DestData, &SourceData](int64 ISource, int64 IDest)
				{
					DestData[IDest] = Pow22OneOver255Table[SourceData[ISource]];
				});
				break;
			default:
				unimplemented()
				break;
			}
		}
		else if constexpr (std::is_same<uint16, TPixelType>::value)
		{
			IterateData(DestTile, SourceTile, [&DestData, &SourceData](int64 ISource, int64 IDest)
			{
				DestData[IDest] = (float)SourceData[ISource] / 65535.f;
			});
		}
		else
		{
			unimplemented()
		}

#undef ITERATE_DATA
	}
}

void FTextureRead::WriteChannel(int32 Channel, int32 Mip, const FTextureDataTileDesc& Tile, float* TextureData) const
{
	if (Channel < ValidChannels)
	{
		check(Mip == 0);
		check(!TextureSourceMip0.IsNull());
		check(Tile.TextureSize.X == Width);
		check(Tile.TextureSize.Y == Height);
		check(Tile.TextureSize.Z == Slices);

		const int SourceChannelIndex = ChannelSwizzle[Channel];

		switch (TextureSourceFormat)
		{
		case TSF_G8:
			CopyImageData<TSF_G8, uint8>(TextureSourceMip0, SourceChannelIndex, TextureSourceGamma, Tile, TextureData);
			break;
		case TSF_BGRA8:
			CopyImageData<TSF_BGRA8, uint8>(TextureSourceMip0, SourceChannelIndex, TextureSourceGamma, Tile, TextureData);
			break;
		case TSF_BGRE8:
			CopyImageData<TSF_BGRE8, uint8>(TextureSourceMip0, SourceChannelIndex, TextureSourceGamma, Tile, TextureData);
			break;
		case TSF_RGBA16:
			CopyImageData<TSF_RGBA16, uint16>(TextureSourceMip0, SourceChannelIndex, TextureSourceGamma, Tile, TextureData);
			break;
		case TSF_RGBA16F:
			CopyImageData<TSF_RGBA16F, FFloat16>(TextureSourceMip0, SourceChannelIndex, TextureSourceGamma, Tile, TextureData);
			break;
		case TSF_G16:
			CopyImageData<TSF_G16, uint16>(TextureSourceMip0, SourceChannelIndex, TextureSourceGamma, Tile, TextureData);
			break;
		case TSF_RGBA32F:
			CopyImageData<TSF_RGBA32F, float>(TextureSourceMip0, SourceChannelIndex, TextureSourceGamma, Tile, TextureData);
			break;
		case TSF_R16F:
			CopyImageData<TSF_R16F, FFloat16>(TextureSourceMip0, SourceChannelIndex, TextureSourceGamma, Tile, TextureData);
			break;
		case TSF_R32F:
			CopyImageData<TSF_R32F, float>(TextureSourceMip0, SourceChannelIndex, TextureSourceGamma, Tile, TextureData);
			break;
		default:
			unimplemented();
			break;
		}
	}
	else
	{
		// Fill tile data with default value
		Tile.ForEachPixel([TextureData, Channel, this](FTextureDataTileDesc::ForEachPixelContext& Context)
		{
			TextureData[Context.DataIndex] = SourceDefinition.DefaultValue[Channel];
		});
	}
}
