
#ifndef __ILTMODEL_H__
#define __ILTMODEL_H__


	#include "ltbasedefs.h"
	#include "ltanimtracker.h"


	class ILTModel
	{
	public:

		// Fills in hSocket with the specified socket (case insensitive).
		virtual LTRESULT GetSocket(HOBJECT hObj, char *pSocketName, HMODELSOCKET &hSocket);

		// Get the specified node (case insensitive).
		virtual LTRESULT GetNode(HOBJECT hObj, char *pNodeName, HMODELNODE &hNode);

		// Get the specified piece (case insensitive).
		virtual LTRESULT GetPiece(HOBJECT hObj, char *pPieceName, HMODELPIECE &hPiece);

		// Get the specified weight set.
		virtual LTRESULT FindWeightSet(HOBJECT hObj, char *pSetName, HMODELWEIGHTSET &hSet);

		// Get the model lighting.
		virtual LTRESULT GetModelLighting(HOBJECT hObj, LTVector & vModelLighting);

		// Hide/unhide a piece on the model (they're all unhidden by default).
		// Only supports the first _32_ pieces in the model.
		// SetPieceHideStatus returns LT_NOCHANGE if the piece is already hidden (not an error).
		virtual LTRESULT GetPieceHideStatus(HOBJECT hObj, HMODELPIECE hPiece, LTBOOL &bHidden);
		virtual LTRESULT SetPieceHideStatus(HOBJECT hObj, HMODELPIECE hPiece, LTBOOL bHidden);


		// Get the transformation for a node on a model.
		// If bWorldSpace is TRUE, then the transform is returned in world space.  Otherwise,
		// the transform is local to the model (based on its position and rotation).
		virtual LTRESULT GetNodeTransform(HOBJECT hObj, HMODELNODE hNode,
			LTransform &transform, LTBOOL bWorldSpace);

		// Fills in pos and rot with the current transform for the socket.
		virtual LTRESULT GetSocketTransform(HOBJECT hObj, HMODELSOCKET hSocket,
			LTransform &transform, LTBOOL bWorldSpace);


		// Get/Set the hook function for a model.  Set it to NULL to disable.
		// NOTE: Setting this on the server doesn't do anything.
		virtual LTRESULT GetInstanceHookFn(HOBJECT hObj, ModelInstanceHookFn &fn);
		virtual LTRESULT SetInstanceHookFn(HOBJECT hObj, ModelInstanceHookFn fn);


        // Get the model's main animation tracker.
		virtual LTRESULT GetMainTracker(HOBJECT hObj, LTAnimTracker* &pTracker);

		// Update the main anim tracker by fUpdateDelta seconds
		// (This is useful for updating the animation on a model outside of a client update)
		virtual LTRESULT UpdateMainTracker(HOBJECT pObj, LTFLOAT fUpdateDelta);


		// Fills in flags with a combination of the MS_ flags in basedefs_de.h.
		virtual LTRESULT GetPlaybackState(LTAnimTracker *pTracker, uint32 &flags);


		// Add/remove trackers on the model.
		virtual LTRESULT AddTracker(HOBJECT hObj, LTAnimTracker *pTracker);
		virtual LTRESULT RemoveTracker(HOBJECT hObj, LTAnimTracker *pTracker);

		// Read/Write trackers
		virtual LTRESULT ReadTracker(LTAnimTracker *pTracker, HMESSAGEREAD hRead);
		virtual LTRESULT WriteTracker(LTAnimTracker *pTracker, HMESSAGEWRITE hWrite);


		// Get/set the current animation for the tracker.
		virtual LTRESULT GetCurAnim(LTAnimTracker *pTracker, HMODELANIM &hAnim);
		virtual LTRESULT SetCurAnim(LTAnimTracker *pTracker, HMODELANIM hAnim);

		// Restart the current animation.
		virtual LTRESULT ResetAnim(LTAnimTracker *pTracker);

		// Returns LT_YES if looping, LT_NO if not looping, or an error.
		virtual LTRESULT GetLooping(LTAnimTracker *pTracker);
		virtual LTRESULT SetLooping(LTAnimTracker *pTracker, LTBOOL bLooping);

		// Returns LT_YES if Playing, LT_NO if not Playing, or an error.
		virtual LTRESULT GetPlaying(LTAnimTracker *pTracker);
		virtual LTRESULT SetPlaying(LTAnimTracker *pTracker, LTBOOL bPlaying);

		virtual LTRESULT GetWeightSet(LTAnimTracker *pTracker, HMODELWEIGHTSET &hSet);
		virtual LTRESULT SetWeightSet(LTAnimTracker *pTracker, HMODELWEIGHTSET hSet);

		virtual LTRESULT GetAllowTransition(LTAnimTracker *pTracker, LTBOOL &bAllowTransition);
		virtual LTRESULT SetAllowTransition(LTAnimTracker *pTracker, LTBOOL bAllowTransition);

		virtual LTRESULT GetTimeScale(LTAnimTracker *pTracker, LTFLOAT & fTimeScale);
		virtual LTRESULT SetTimeScale(LTAnimTracker *pTracker, LTFLOAT   fTimeScale);
		
		// Gets the current animation's length (in milliseconds).
		// If bIncorporateScale is true, the length returned accounts for the tracker's time scale.
		virtual LTRESULT GetCurAnimLength(LTAnimTracker *pTracker, uint32 &length, LTBOOL bIncorporateScale = LTTRUE);
		
		// Get/Set where the model is in its animation (in milliseconds).
		// If bIncorporateScale is true, the curTime returned accounts for the tracker's time scale.
		virtual LTRESULT GetCurAnimTime(LTAnimTracker *pTracker, uint32 &curTime, LTBOOL bIncorporateScale = LTTRUE);
		virtual LTRESULT SetCurAnimTime(LTAnimTracker *pTracker, uint32 curTime, LTBOOL bIncorporateScale = LTTRUE);

		// Returns LT_YES if looping, LT_NO if not looping, or an error.
		virtual LTRESULT GetNormalize(LTAnimTracker *pTracker);
		virtual LTRESULT SetNormalize(LTAnimTracker *pTracker, LTBOOL bNormalize);

		// Ray intersection parameters/result
		struct LTRayResult {
			// Input
			LTVector m_vOrigin, m_vDir;
			LTFLOAT m_fMinDist, m_fMaxDist;
			// Output
			LTBOOL m_bIntersect;
			LTVector m_vNormal;
			LTFLOAT m_fDistance;
			HMODELPIECE m_nPiece;
		};
		// Finds a set of ray intersections with a model
		virtual LTRESULT IntersectRays(
			HOBJECT hModel, // The model to intersect
			const LTVector &vCamPos, // The position of the camera (for LOD selection)
			int32 nLODOffset, // LOD offset to use
			HMODELPIECE *aPieces, uint32 nPieceCount, // Pieces to use for reduction of the set of tris.  NULL/0 = all pieces
			LTRayResult *aRays, uint32 nRayCount // The rays to intersect, and their results
		);
	};


#endif // __ILTMODEL_H__
