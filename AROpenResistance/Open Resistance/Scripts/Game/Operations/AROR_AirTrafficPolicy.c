//! Predict closest separation during a short constant-velocity lookahead; no flight teleporting.
class AROR_AirTrafficPolicy
{
	static float PredictedSeparation(vector position, vector velocity, vector otherPosition, vector otherVelocity, float horizon)
	{
		vector delta = position - otherPosition;
		vector relative = velocity - otherVelocity;
		float speedSquared = relative[0] * relative[0] + relative[1] * relative[1] + relative[2] * relative[2];
		float time = 0;
		if (speedSquared > 0.01)
		{
			float dot = delta[0] * relative[0] + delta[1] * relative[1] + delta[2] * relative[2];
			time = Math.Clamp(-dot / speedSquared, 0, horizon);
		}
		return (delta + relative * time).Length();
	}
}
