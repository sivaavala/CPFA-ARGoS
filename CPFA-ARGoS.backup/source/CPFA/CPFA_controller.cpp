#include "CPFA_controller.h"
#include <unistd.h>

CPFA_controller::CPFA_controller() :
    RNG(argos::CRandom::CreateRNG("argos")),
    isInformed(false),
    isHoldingFood(false),
    isUsingSiteFidelity(false),
    isGivingUpSearch(false),
    ResourceDensity(0),
    MaxTrailSize(50),
    SearchTime(0),
    CPFA_state(DEPARTING),
    LoopFunctions(NULL),
    survey_count(0)
{}

void CPFA_controller::Init(argos::TConfigurationNode &node) {

    compassSensor   = GetSensor<argos::CCI_PositioningSensor>("positioning");
    wheelActuator   = GetActuator<argos::CCI_DifferentialSteeringActuator>("differential_steering");
    proximitySensor = GetSensor<argos::CCI_FootBotProximitySensor>("footbot_proximity");
    argos::TConfigurationNode settings = argos::GetNode(node, "settings");
    argos::GetNodeAttribute(settings, "FoodDistanceTolerance",   FoodDistanceTolerance);
    argos::GetNodeAttribute(settings, "TargetDistanceTolerance", TargetDistanceTolerance);
    argos::GetNodeAttribute(settings, "TargetAngleTolerance",    TargetAngleTolerance);
    argos::GetNodeAttribute(settings, "SearchStepSize",          SearchStepSize);
    argos::GetNodeAttribute(settings, "RobotForwardSpeed",       RobotForwardSpeed);
    argos::GetNodeAttribute(settings, "RobotRotationSpeed",      RobotRotationSpeed);
    argos::GetNodeAttribute(settings, "ResultsDirectoryPath",      results_path);

    argos::CVector2 p(GetPosition());
    SetStartPosition(argos::CVector3(p.GetX(), p.GetY(), 0.0));

    FoodDistanceTolerance *= FoodDistanceTolerance;

}

void CPFA_controller::ControlStep() {

  /*
  ofstream log_output_stream;
  log_output_stream.open("cpfa_log.txt", ios::app);

      // depart from nest after food drop off or simulation start
      if (isHoldingFood)      
	log_output_stream << "(Carrying) ";
  
  switch(CPFA_state) 
    {
    case DEPARTING:
      log_output_stream << "DEPARTING" << endl;
      break;
      // after departing(), once conditions are met, begin searching()
    case SEARCHING:
      if (isInformed) 
	if (!isUsingSiteFidelity)
	  log_output_stream << "SEARCHING: Informed (P)" << endl;
	else
	  log_output_stream << "SEARCHING: Informed (F)" << endl;
      else
	log_output_stream << "SEARCHING: UnInformed" << endl;
      break;
      // return to nest after food pick up or giving up searching()
    case RETURNING:
      log_output_stream << "RETURNING" << endl;
      break;
   case SURVEYING:
      log_output_stream << "SURVEYING" << endl;
      break;
    default:
      log_output_stream << "Unknown state" << endl;
    }
  */
  

  // Add line so we can draw the trail

  CVector3 position3d(GetPosition().GetX(), GetPosition().GetY(), 0.00);
  CVector3 target3d(previous_position.GetX(), previous_position.GetY(), 0.00);
  CRay3 targetRay(target3d, position3d);
  myTrail.push_back(targetRay);
  LoopFunctions->TargetRayList.push_back(targetRay);
  LoopFunctions->TargetRayColorList.push_back(TrailColor);

  previous_position = GetPosition();

  //UpdateTargetRayList();
  
    CPFA();
    Move();
}

void CPFA_controller::Reset() {
}

bool CPFA_controller::IsHoldingFood() {
    return isHoldingFood;
}

bool CPFA_controller::IsUsingSiteFidelity() {
    return isUsingSiteFidelity;
}

void CPFA_controller::CPFA() {
  
  switch(CPFA_state) {
    // depart from nest after food drop off or simulation start
  case DEPARTING:
    Departing();
    break;
    // after departing(), once conditions are met, begin searching()
  case SEARCHING:
    if((SimulationTick() % (SimulationTicksPerSecond() / 2)) == 0)
      Searching();
    break;
    // return to nest after food pick up or giving up searching()
  case RETURNING:
    Returning();
    break;
  case SURVEYING:
    Surveying();
    break;
  }
}

/*****
 *
 *****/
bool CPFA_controller::IsInTheNest() {
    return ((GetPosition() - LoopFunctions->NestPosition).SquareLength() < LoopFunctions->NestRadiusSquared);
}

void CPFA_controller::SetLoopFunctions(CPFA_loop_functions* lf) {
    LoopFunctions = lf;


    // Create the output file here because it needs LoopFunctions
    
    // Name the results file with the current time and date
 time_t t = time(0);   // get time now
    struct tm * now = localtime( & t );
    stringstream ss;

char hostname[1024];                                                   
hostname[1023] = '\0';                    
gethostname(hostname, 1023);  

    ss << "CPFA-"<<GIT_BRANCH<<"-"<<GIT_COMMIT_HASH<<"-"
       << hostname << '-'
       << getpid() << '-'
       << (now->tm_year) << '-'
       << (now->tm_mon + 1) << '-'
       <<  now->tm_mday << '-'
       <<  now->tm_hour << '-'
       <<  now->tm_min << '-'
       <<  now->tm_sec << ".csv";

    string results_file_name = ss.str();
   results_full_path = results_path+"/"+results_file_name;        


    // Only the first robot should do this:
   
    if (GetId().compare("CPFA_0") == 0)
      {

   ofstream results_output_stream;
   results_output_stream.open(results_full_path, ios::app);
   results_output_stream << "NumberOfRobots, "
			 << "TargetDistanceTolerance, "
			 << "TargetAngleTolerance, "
			 << "FoodDistanceTolerance, "
			 << "RobotForwardSpeed, "
			 << "RobotRotationSpeed, "
			 << "RandomSeed, "
			 << "ProbabilityOfSwitchingToSearching, "
			 << "ProbabilityOfReturningToNest, "
			 << "UninformedSearchVariation, "   
			 << "RateOfInformedSearchDecay, "   
			 << "RateOfSiteFidelity, "          
			 << "RateOfLayingPheromone, "       
			 << "RateOfPheromoneDecay" << endl
			 << LoopFunctions->getNumberOfRobots() << ", "
			 << CSimulator::GetInstance().GetRandomSeed() << ", "  
     			 << TargetDistanceTolerance << ", "
     			 << TargetAngleTolerance << ", "
     			 << FoodDistanceTolerance << ", "
     			 << RobotForwardSpeed << ", "
     			 << RobotRotationSpeed << ", "
     			 << LoopFunctions->getProbabilityOfSwitchingToSearching() << ", "
     			 << LoopFunctions->getProbabilityOfReturningToNest() << ", "
     			 << LoopFunctions->getUninformedSearchVariation() << ", "
     			 << LoopFunctions->getRateOfInformedSearchDecay() << ", "
     			 << LoopFunctions->getRateOfSiteFidelity() << ", "
     			 << LoopFunctions->getRateOfLayingPheromone() << ", "
     			 << LoopFunctions->getRateOfPheromoneDecay()
			 << endl;
     		
   results_output_stream.close();
      }

}

void CPFA_controller::Departing() {
    argos::Real distanceToTarget = (GetPosition() - GetTarget()).Length();
    argos::Real randomNumber = RNG->Uniform(argos::CRange<argos::Real>(0.0, 1.0));

    /* When not informed, continue to travel until randomly switching to the searching state. */
    if((SimulationTick() % (SimulationTicksPerSecond() / 2)) == 0) {
    if(!isInformed && randomNumber < LoopFunctions->ProbabilityOfSwitchingToSearching) 
	{
	  SearchTime = 0;
	  CPFA_state = SEARCHING;
	  argos::Real USV = LoopFunctions->UninformedSearchVariation.GetValue();
	  argos::Real rand = RNG->Gaussian(USV);
	  argos::CRadians rotation(rand);
	  argos::CRadians angle1(rotation.UnsignedNormalize());
	  argos::CRadians angle2(GetHeading().UnsignedNormalize());
	  argos::CRadians turn_angle(angle1 + angle2);
	  argos::CVector2 turn_vector(SearchStepSize, turn_angle);
	  
	  SetTarget(turn_vector + GetPosition());
	}
    }
    /* Are we informed? I.E. using site fidelity or pheromones. */
      

    if(isInformed && distanceToTarget < TargetDistanceTolerance) {
        SearchTime = 0;
        CPFA_state = SEARCHING;
	
        if(isUsingSiteFidelity == true) {
            isUsingSiteFidelity = false;
            SetFidelityList();
        }
    }

}

void CPFA_controller::Searching() {
    // "scan" for food only every half of a second
    //if((SimulationTick() % (SimulationTicksPerSecond() / 2)) == 0) {
        SetHoldingFood();
	//}

    // When not carrying food, calculate movement.
    if(IsHoldingFood() == false) {
        argos::CVector2 distance = GetPosition() - GetTarget();
        argos::Real     random   = RNG->Uniform(argos::CRange<argos::Real>(0.0, 1.0));

        // If we reached our target search location, set a new one. The 
        // new search location calculation is different based on whether
        // we are currently using informed or uninformed search.
        if(distance.SquareLength() < TargetDistanceTolerance) {

	  // randomly give up searching
	  if(random < LoopFunctions->ProbabilityOfReturningToNest) {
            SetTarget(LoopFunctions->NestPosition);
            isGivingUpSearch = true;
            CPFA_state = RETURNING;
	    
	    /*
	    ofstream log_output_stream;
	    log_output_stream.open("giveup.txt", ios::app);
	    log_output_stream << "Give up: " << SimulationTick() / SimulationTicksPerSecond() << endl;
	    log_output_stream.close();
	    */

	    return;
	  }

	  // uninformed search
	  if(isInformed == false) {
                argos::Real USCV = LoopFunctions->UninformedSearchVariation.GetValue();
                argos::Real rand = RNG->Gaussian(USCV);
                argos::CRadians rotation(rand);
                argos::CRadians angle1(rotation.UnsignedNormalize());
                argos::CRadians angle2(GetHeading().UnsignedNormalize());
                argos::CRadians turn_angle(angle1 + angle2);
                argos::CVector2 turn_vector(SearchStepSize, turn_angle);

                SetTarget(turn_vector + GetPosition());
            }
            // informed search
            else if(isInformed == true) {
                size_t          t           = SearchTime++;
                argos::Real     twoPi       = (argos::CRadians::TWO_PI).GetValue();
                argos::Real     pi          = (argos::CRadians::PI).GetValue();
                argos::Real     isd         = LoopFunctions->RateOfInformedSearchDecay;
                argos::Real     correlation = GetExponentialDecay((2.0 * twoPi) - LoopFunctions->UninformedSearchVariation.GetValue(), t, isd);
                argos::Real     rand = RNG->Gaussian(correlation + LoopFunctions->UninformedSearchVariation.GetValue());
                argos::CRadians rotation(GetBound(rand, -pi, pi));
                argos::CRadians angle1(rotation);
                argos::CRadians angle2(GetHeading());
                argos::CRadians turn_angle(angle2 + angle1);
                argos::CVector2 turn_vector(SearchStepSize, turn_angle);

                SetTarget(turn_vector + GetPosition());
            }
        }
    }
    // Food has been found, change state to RETURNING and go to the nest
    //else {
    //    SetTarget(LoopFunctions->NestPosition);
    //    CPFA_state = RETURNING;
    // }
}

// Cause the robot to rotate in place as if surveying the surrounding targets
// Turns 36 times by 10 degrees
void CPFA_controller::Surveying() 
{
  if (survey_count <= 4)
    { 
      CRadians rotation(survey_count*3.14/2); // divide by 10 so the vecot is small and the linear motion is minimized
      argos::CVector2 turn_vector(SearchStepSize, rotation.SignedNormalize());
      
      SetTarget(turn_vector + GetPosition());
/*
      ofstream log_output_stream;
      log_output_stream.open("log.txt", ios::app);
      log_output_stream << (GetHeading() - rotation ).SignedNormalize() << ", "  << SearchStepSize << ", "<< rotation << ", " <<  turn_vector << ", " << GetHeading() << ", " << survey_count << endl;
      log_output_stream.close();
*/
      if ( fabs((GetHeading() - rotation ).SignedNormalize().GetValue())  < TargetAngleTolerance.GetValue() )
	survey_count++;
      //else Keep trying to reach the turning angle
    }
  else // Set the survey countdown
    {
      SetTarget(LoopFunctions->NestPosition);
      CPFA_state = RETURNING;
      survey_count = 0; // Reset
    }
}


/*****
 * RETURNING: Stay in this state until the robot has returned to the nest.
 * This state is triggered when a robot has found food or when it has given
 * up on searching and is returning to the nest.
 *****/
void CPFA_controller::Returning() {

  //SetHoldingFood();
    
    argos::CVector2 distance = GetPosition() - GetTarget();

    // Are we there yet? (To the nest, that is.)
    if(IsInTheNest() == true) {
        // Based on a Poisson CDF, the robot may or may not create a pheromone
        // located at the last place it picked up food.
        argos::Real poissonCDF_pLayRate    = GetPoissonCDF(ResourceDensity, LoopFunctions->RateOfLayingPheromone);
        argos::Real poissonCDF_sFollowRate = GetPoissonCDF(ResourceDensity, LoopFunctions->RateOfSiteFidelity);
        argos::Real r1 = RNG->Uniform(argos::CRange<argos::Real>(0.0, 1.0));
        argos::Real r2 = RNG->Uniform(argos::CRange<argos::Real>(0.0, 1.0));

        if(poissonCDF_pLayRate > r1) {
            if(isGivingUpSearch == false) {
                TrailToShare.push_back(LoopFunctions->NestPosition);
                argos::Real timeInSeconds = (argos::Real)(SimulationTick() / SimulationTicksPerSecond());
                Pheromone sharedPheromone(SiteFidelityPosition, TrailToShare, timeInSeconds, LoopFunctions->RateOfPheromoneDecay);
                LoopFunctions->PheromoneList.push_back(sharedPheromone);
                TrailToShare.clear();
                sharedPheromone.Deactivate(); // make sure this won't get re-added later...
            } else {
                isGivingUpSearch = false;
            }
        }

        // Determine probabilistically wether to use site fidelity, pheromone
        // trails, or random search.

        // use site fidelity
        if((isUsingSiteFidelity == true) && (poissonCDF_sFollowRate > r2)) {
            SetTarget(SiteFidelityPosition);
            isInformed = true;
        }
        // use pheromone waypoints
        else if(SetTargetPheromone() == true) {
            isInformed = true;
            isUsingSiteFidelity = false;
        }
        // use random search
        else {
            SetRandomSearchLocation();
            isInformed = false;
            isUsingSiteFidelity = false;
        }

	// Record that a target has been retrieved
	if (isHoldingFood)
	  {
	    ofstream results_output_stream;
	    results_output_stream.open(results_full_path, ios::app);
	    results_output_stream << LoopFunctions->getSimTimeInSeconds() << ", " << ++num_targets_collected << endl;	    
	    results_output_stream.close();

	    LoopFunctions->setScore(num_targets_collected);

	    isHoldingFood = false;
	    CPFA_state = DEPARTING;
	    
	     // We dropped off food. Clear the built-up pheromone trail.
	    TrailToShare.clear();
	  }
    }

    //if(distance.SquareLength() < TargetDistanceTolerance) {
    //
    //   argos::CVector2 step(SearchStepSize, GetHeading().UnsignedNormalize());
    //
    //    SetTarget(GetPosition() + step);
    //}

}

void CPFA_controller::SetRandomSearchLocation() {
    argos::Real random_wall = RNG->Uniform(argos::CRange<argos::Real>(0.0, 1.0));
    argos::Real x = 0.0, y = 0.0;

    /* north wall */
    if(random_wall < 0.25) {
        x = RNG->Uniform(ForageRangeX);
        y = ForageRangeY.GetMax();
    }
    /* south wall */
    else if(random_wall < 0.5) {
        x = RNG->Uniform(ForageRangeX);
        y = ForageRangeY.GetMin();
    }
    /* east wall */
    else if(random_wall < 0.75) {
        x = ForageRangeX.GetMax();
        y = RNG->Uniform(ForageRangeY);
    }
    /* west wall */
    else {
        x = ForageRangeX.GetMin();
        y = RNG->Uniform(ForageRangeY);
    }

    SetTarget(argos::CVector2(x, y));
}

/*****
 * Check if the iAnt is finding food. This is defined as the iAnt being within
 * the distance tolerance of the position of a food item. If the iAnt has found
 * food then the appropriate boolean flags are triggered.
 *****/
void CPFA_controller::SetHoldingFood() {

    // Is the iAnt already holding food?
    if(IsHoldingFood() == false) {

        // No, the iAnt isn't holding food. Check if we have found food at our
        // current position and update the food list if we have.

        std::vector<argos::CVector2> newFoodList;
        std::vector<argos::CColor> newFoodColoringList;
        size_t i = 0, j = 0;

        for(i = 0; i < LoopFunctions->FoodList.size(); i++) {
            if((GetPosition() - LoopFunctions->FoodList[i]).SquareLength() < FoodDistanceTolerance) {
                // We found food! Calculate the nearby food density.
                isHoldingFood = true;
		CPFA_state = SURVEYING;
                j = i + 1;
                break;
            } else {
                //Return this unfound-food position to the list
                newFoodList.push_back(LoopFunctions->FoodList[i]);
                newFoodColoringList.push_back(LoopFunctions->FoodColoringList[i]);
            }
        }

        for( ; j < LoopFunctions->FoodList.size(); j++) {
            newFoodList.push_back(LoopFunctions->FoodList[j]);
            newFoodColoringList.push_back(LoopFunctions->FoodColoringList[j]);
        }

        // We picked up food. Update the food list minus what we picked up.
        if(IsHoldingFood() == true) {
	  SetTarget(LoopFunctions->NestPosition);
            LoopFunctions->FoodList = newFoodList;
            SetLocalResourceDensity();
        }
       
    }
    
    // This shouldn't be checked here ---
    // Drop off food: We are holding food and have reached the nest.
    //else if((GetPosition() - LoopFunctions->NestPosition).SquareLength() < LoopFunctions->NestRadiusSquared) {
    //    isHoldingFood = false;
    // }

    // We are carrying food and haven't reached the nest, keep building up the
    // pheromone trail attached to this found food item.
    if(IsHoldingFood() == true && SimulationTick() % LoopFunctions->DrawDensityRate == 0) {
        TrailToShare.push_back(GetPosition());
    }

}

/*****
 * If the robot has just picked up a food item, this function will be called
 * so that the food density in the local region is analyzed and saved. This
 * helps facilitate calculations for pheromone laying.
 *
 * Ideally, given that: [*] is food, and [=] is a robot
 *
 * [*] [*] [*] | The maximum resource density that should be calculated is
 * [*] [=] [*] | equal to 9, counting the food that the robot just picked up
 * [*] [*] [*] | and up to 8 of its neighbors.
 *
 * That being said, the random and non-grid nature of movement will not
 * produce the ideal result most of the time. This is especially true since
 * item detection is based on distance calculations with circles.
 *****/
void CPFA_controller::SetLocalResourceDensity() {

    argos::CVector2 distance;
    ResourceDensity = 1; // remember: the food we picked up is removed from the foodList before this function call
                         // therefore compensate here by counting that food (which we want to count)

    /* Calculate resource density based on the global food list positions. */
    for(size_t i = 0; i < LoopFunctions->FoodList.size(); i++) {
        distance = GetPosition() - LoopFunctions->FoodList[i];

        if(distance.SquareLength() < LoopFunctions->SearchRadiusSquared) {
            ResourceDensity++;
            LoopFunctions->FoodColoringList[i] = argos::CColor::ORANGE;
            LoopFunctions->ResourceDensityDelay = SimulationTick() + SimulationTicksPerSecond() * 10;
        }
    }

    /* Set the fidelity position to the robot's current position. */
    SiteFidelityPosition = GetPosition();
    isUsingSiteFidelity = true;

    /* Delay for 4 seconds (simulate iAnts scannning rotation). */
    //  Wait(4); // This function is broken. It causes the rover to move in the wrong direction after finishing its local resource density test 
}

/*****
 * Update the global site fidelity list for graphics display and add a new fidelity position.
 *****/
void CPFA_controller::SetFidelityList(argos::CVector2 newFidelity) {

    std::vector<argos::CVector2> newFidelityList;

    /* Remove this robot's old fidelity position from the fidelity list. */
    for(size_t i = 0; i < LoopFunctions->FidelityList.size(); i++) {
        if((LoopFunctions->FidelityList[i] - SiteFidelityPosition).SquareLength() != 0.0)
            newFidelityList.push_back(LoopFunctions->FidelityList[i]);
    }

    /* Update the global fidelity list. */
    LoopFunctions->FidelityList = newFidelityList;

    /* Add the robot's new fidelity position to the global fidelity list. */
    LoopFunctions->FidelityList.push_back(newFidelity);

    /* Update the local fidelity position for this robot. */
    SiteFidelityPosition = newFidelity;
}

/*****
 * Update the global site fidelity list for graphics display and remove the old fidelity position.
 *****/
void CPFA_controller::SetFidelityList() {

    std::vector<argos::CVector2> newFidelityList;

    /* Remove this robot's old fidelity position from the fidelity list. */
    for(size_t i = 0; i < LoopFunctions->FidelityList.size(); i++) {
        if((LoopFunctions->FidelityList[i] - SiteFidelityPosition).SquareLength() != 0.0)
            newFidelityList.push_back(LoopFunctions->FidelityList[i]);
    }

    /* Update the global fidelity list. */
    LoopFunctions->FidelityList = newFidelityList;
}

/*****
 * Update the pheromone list and set the target to a pheromone position.
 * return TRUE:  pheromone was successfully targeted
 *        FALSE: pheromones don't exist or are all inactive
 *****/
bool CPFA_controller::SetTargetPheromone() {
    argos::Real maxStrength = 0.0, randomWeight = 0.0;
    bool isPheromoneSet = false;

    /* update the pheromone list and remove inactive pheromones */
    // LoopFunctions->UpdatePheromoneList();

    /* default target = nest; in case we have 0 active pheromones */
    SetTarget(LoopFunctions->NestPosition);

    /* Calculate a maximum strength based on active pheromone weights. */
    for(size_t i = 0; i < LoopFunctions->PheromoneList.size(); i++) {
        if(LoopFunctions->PheromoneList[i].IsActive() == true) {
            maxStrength += LoopFunctions->PheromoneList[i].GetWeight();
        }
    }

    /* Calculate a random weight. */
    randomWeight = RNG->Uniform(argos::CRange<argos::Real>(0.0, maxStrength));

    /* Randomly select an active pheromone to follow. */
    for(size_t i = 0; i < LoopFunctions->PheromoneList.size(); i++) {
        if(randomWeight < LoopFunctions->PheromoneList[i].GetWeight()) {
            /* We've chosen a pheromone! */
            SetTarget(LoopFunctions->PheromoneList[i].GetLocation());
            TrailToFollow = LoopFunctions->PheromoneList[i].GetTrail();
            isPheromoneSet = true;
            /* If we pick a pheromone, break out of this loop. */
            break;
        }

        /* We didn't pick a pheromone! Remove its weight from randomWeight. */
        randomWeight -= LoopFunctions->PheromoneList[i].GetWeight();
    }

    return isPheromoneSet;
}

/*****
 * Calculate and return the exponential decay of "value."
 *****/
argos::Real CPFA_controller::GetExponentialDecay(argos::Real value, argos::Real time, argos::Real lambda) {

    /* convert time into units of haLoopFunctions-seconds from simulation frames */
    //time = time / (LoopFunctions->TicksPerSecond / 2.0);

    //LOG << "time: " << time << endl;
    //LOG << "correlation: " << (value * exp(-lambda * time)) << endl << endl;

    return (value * std::exp(-lambda * time));
}

/*****
 * Provides a bound on the value by rolling over a la modulo.
 *****/
argos::Real CPFA_controller::GetBound(argos::Real value, argos::Real min, argos::Real max) {
    /* Calculate an offset. */
    argos::Real offset = std::abs(min) + std::abs(max);

    /* Increment value by the offset while it's less than min. */
    while (value < min) {
        value += offset;
    }

    /* Decrement value by the offset while it's greater than max. */
    while (value > max) {
        value -= offset;
    }

    /* Return the bounded value. */
    return value;
}

/*****
 * Return the Poisson cumulative probability at a given k and lambda.
 *****/
argos::Real CPFA_controller::GetPoissonCDF(argos::Real k, argos::Real lambda) {
    argos::Real sumAccumulator       = 1.0;
    argos::Real factorialAccumulator = 1.0;

    for (size_t i = 1; i <= floor(k); i++) {
        factorialAccumulator *= i;
        sumAccumulator += pow(lambda, i) / factorialAccumulator;
    }

    return (exp(-lambda) * sumAccumulator);
}

void CPFA_controller::UpdateTargetRayList() {
    if(SimulationTick() % LoopFunctions->DrawDensityRate == 0 && LoopFunctions->DrawTargetRays == 1) {
        /* Get position values required to construct a new ray */
        argos::CVector2 t(GetTarget());
        argos::CVector2 p(GetPosition());
        argos::CVector3 position3d(p.GetX(), p.GetY(), 0.02);
        argos::CVector3 target3d(t.GetX(), t.GetY(), 0.02);

        /* scale the target ray to be <= searchStepSize */
        argos::Real length = std::abs(t.Length() - p.Length());

        if(length > SearchStepSize) {
            MyTrail.clear();
        } else {
            /* add the ray to the robot's target trail */
            argos::CRay3 targetRay(target3d, position3d);
            MyTrail.push_back(targetRay);

            /* delete the oldest ray from the trail */
            if(MyTrail.size() > MaxTrailSize) {
                MyTrail.erase(MyTrail.begin());
            }

            LoopFunctions->TargetRayList.insert(LoopFunctions->TargetRayList.end(), MyTrail.begin(), MyTrail.end());
            // loopFunctions.TargetRayList.push_back(myTrail);
        }
    }
}

REGISTER_CONTROLLER(CPFA_controller, "CPFA_controller")