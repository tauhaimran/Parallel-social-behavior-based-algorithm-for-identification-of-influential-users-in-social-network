# Parallel social behavior-based algorithm for identification of influential users in social network
 PDC - semester project
    Tauha Imran
    Muhammad Tahir
    Sameed Ahmed

 This Project explores the problem of *influence maximization* in social networks, which involves finding the most influential users to spread information effectively. Here’s a condensed summary of its core ideas:

### Motivation
- Existing methods mainly focus on structural properties of networks, ignoring user *interests* and *interactive behaviors*.
- Large-scale networks create challenges due to their size and complexity.

### Proposed Solution: *PSAIIM Algorithm*
- Introduces a new parallel algorithm that incorporates user interests, weighted social actions, and network structure to identify influential users.
- Leverages *community detection* and CPU parallelism to improve scalability and execution speed.

### Key Contributions
1. *Behavior Integration*: Considers user social behavior, such as their interactions (likes, shares, etc.) and areas of interest.
2. *Two-Criteria Model*: Evaluates influence based on network topology and semantic attributes.
3. *Parallelization*: Optimizes computation using CPU threading and community-based network partitioning.

### Results
- Experimental analysis shows PSAIIM outperforms traditional methods in speed and effectiveness, especially for large-scale social networks.

### Applications
- Marketing: Target influential users to maximize product adoption.
- Epidemic Control: Analyze diffusion dynamics to prevent outbreaks.
- Political Campaigns: Identify key nodes for spreading campaign messages.

Would you like details on any specific section? Or maybe explore how these concepts can apply to real-world scenarios? 😊
